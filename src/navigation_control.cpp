#include "navigation_control.h"
#include "logger.h"
#include "watchdog.h"
#include <chrono>
#include <cmath>
#include <limits>
#include <pthread.h>
#include <cstring>

NavigationControl::NavigationControl(CircularBuffer& buffer, int period_ms)
    : buffer_(buffer),
      period_ms_(period_ms),
      running_(false),
      previous_distance_(std::numeric_limits<double>::max()) {

    truck_state_.fault = false;
    truck_state_.automatic = false;
    output_.arrived = false;
    output_.acceleration = 0;
    output_.steering = 0;

    LOG_INFO(NC) << "event" << "init" << "period_ms" << period_ms_;
}

NavigationControl::~NavigationControl() {
    stop();
}

void NavigationControl::start() {
    if (running_) {
        return;
    }

    running_ = true;
    task_thread_ = std::thread(&NavigationControl::task_loop, this);

    // Set real-time scheduling priority (medium priority for navigation control)
    // SCHED_FIFO ensures deterministic scheduling for hard real-time requirements
    pthread_t native_handle = task_thread_.native_handle();
    struct sched_param param;
    param.sched_priority = 70; // Priority: 70 (medium, lower than command logic)

    int result = pthread_setschedparam(native_handle, SCHED_FIFO, &param);
    if (result == 0) {
        LOG_INFO(NC) << "event" << "start" << "rt_priority" << 70 << "sched" << "FIFO";
    } else {
        LOG_WARN(NC) << "event" << "start" << "rt_priority" << "failed" << "errno" << result;
    }
}

void NavigationControl::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    LOG_INFO(NC) << "event" << "stop";
}

void NavigationControl::set_setpoint(const NavigationSetpoint& setpoint) {
    std::lock_guard<std::mutex> lock(control_mutex_);

    bool new_target = (setpoint.target_position_x != setpoint_.target_position_x) ||
                     (setpoint.target_position_y != setpoint_.target_position_y);

    setpoint_ = setpoint;

    if (new_target) {
        previous_distance_ = std::numeric_limits<double>::max();
        output_.arrived = false;
    }
}

void NavigationControl::set_truck_state(const TruckState& state) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    truck_state_ = state;
}

ActuatorOutput NavigationControl::get_output() const {
    std::lock_guard<std::mutex> lock(control_mutex_);
    return output_;
}

void NavigationControl::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {

        SensorData sensor_data = buffer_.peek_latest();

        {
            std::lock_guard<std::mutex> lock(control_mutex_);


            bool controllers_enabled = truck_state_.automatic && !truck_state_.fault;

            if (controllers_enabled) {
                if (output_.arrived) {

                    output_.acceleration = 0;
                    output_.steering = 0;
                } else {

                    int dx = setpoint_.target_position_x - sensor_data.position_x;
                    int dy = setpoint_.target_position_y - sensor_data.position_y;
                    double distance = std::sqrt(dx*dx + dy*dy);

                    int angle_error = setpoint_.target_angle - sensor_data.angle_x;
                    while (angle_error > 180) angle_error -= 360;
                    while (angle_error < -180) angle_error += 360;
                    double abs_angle_error = std::abs(angle_error);

                    bool distance_increasing = distance > previous_distance_ &&
                                              previous_distance_ < ARRIVAL_DISTANCE_THRESHOLD * 2;


                    previous_distance_ = distance;

                    bool arrival_condition = (distance <= ARRIVAL_DISTANCE_THRESHOLD &&
                                             abs_angle_error <= ARRIVAL_ANGLE_THRESHOLD) ||
                                            (distance_increasing && distance < ARRIVAL_DISTANCE_THRESHOLD * 1.5);

                    if (arrival_condition) {
                        output_.arrived = true;
                        output_.acceleration = 0;
                        output_.steering = 0;

                        LOG_INFO(NC) << "event" << "arrived"
                                     << "dist" << static_cast<int>(distance)
                                     << "ang_err" << static_cast<int>(abs_angle_error)
                                     << "overshoot" << (distance_increasing ? 1 : 0);

                    } else {
                        output_.acceleration = speed_controller(
                            sensor_data.position_x, sensor_data.position_y,
                            setpoint_.target_position_x, setpoint_.target_position_y
                        );

                        output_.steering = angle_controller(
                            sensor_data.angle_x,
                            setpoint_.target_angle
                        );
                    }
                }
            } else {
                setpoint_.target_position_x = sensor_data.position_x;
                setpoint_.target_position_y = sensor_data.position_y;
                setpoint_.target_angle = sensor_data.angle_x;

                output_.acceleration = 0;
                output_.steering = 0;
                output_.arrived = false;
            }
        }

        // Report heartbeat to watchdog
        if (Watchdog::get_instance()) {
            Watchdog::get_instance()->heartbeat("NavigationControl");
        }


        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

int NavigationControl::speed_controller(int current_x, int current_y,
                                        int target_x, int target_y) {

    int dx = target_x - current_x;
    int dy = target_y - current_y;
    double distance = std::sqrt(dx*dx + dy*dy);

    int control_output;

    if (distance < 2.0) {
        // Very close to target - stop completely
        control_output = 0;
    } else if (distance < 12.0) {
        // Extended close zone - aggressive deceleration
        double deceleration_factor = distance / 12.0;
        control_output = static_cast<int>(10 * deceleration_factor);
    } else if (distance < DECELERATION_DISTANCE) {
        // Within deceleration zone - reduce acceleration aggressively
        double deceleration_factor = distance / DECELERATION_DISTANCE;
        // Use even more aggressive curve for deceleration
        deceleration_factor = std::pow(deceleration_factor, 1.5); // More aggressive than sqrt
        control_output = static_cast<int>(MAX_ACCELERATION * deceleration_factor * 0.4);
    } else {
        // Far from target - constant acceleration
        control_output = MAX_ACCELERATION;
    }

    // Clamp output to reasonable range
    if (control_output > 100) control_output = 100;
    if (control_output < 0) control_output = 0; // No reverse acceleration

    return control_output;
}

int NavigationControl::angle_controller(int current_angle, int target_angle) {

    int error = target_angle - current_angle;


    while (error > 180) error -= 360;
    while (error < -180) error += 360;


    int control_output = static_cast<int>(KP_ANGLE * error);

    if (control_output > 80) control_output = 80;
    if (control_output < -80) control_output = -80;

    return control_output;
}
