#include "navigation_control.h"
#include "logger.h"
#include <chrono>
#include <cmath>
#include <limits>

NavigationControl::NavigationControl(CircularBuffer& buffer, int period_ms)
    : buffer_(buffer),
      period_ms_(period_ms),
      running_(false),
      previous_distance_(std::numeric_limits<double>::max()) {

    // Initialize with safe defaults
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

    LOG_INFO(NC) << "event" << "start";
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

    // Check if this is a new target position
    bool new_target = (setpoint.target_position_x != setpoint_.target_position_x) ||
                     (setpoint.target_position_y != setpoint_.target_position_y);

    setpoint_ = setpoint;

    // Reset distance tracking for new target
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
        // Peek at latest sensor data
        SensorData sensor_data = buffer_.peek_latest();

        {
            std::lock_guard<std::mutex> lock(control_mutex_);

            // Check if controllers should be active
            bool controllers_enabled = truck_state_.automatic && !truck_state_.fault;

            if (controllers_enabled) {
                // If we have already arrived, do nothing.
                // The 'arrived' flag is reset by set_setpoint when a new target is given.
                if (output_.arrived) {
                    // Keep motors off.
                    output_.acceleration = 0;
                    output_.steering = 0;
                } else {
                    // NOT ARRIVED YET: Run navigation logic.
                    int dx = setpoint_.target_position_x - sensor_data.position_x;
                    int dy = setpoint_.target_position_y - sensor_data.position_y;
                    double distance = std::sqrt(dx*dx + dy*dy);

                    int angle_error = setpoint_.target_angle - sensor_data.angle_x;
                    // Normalize angle error to [-180, 180]
                    while (angle_error > 180) angle_error -= 360;
                    while (angle_error < -180) angle_error += 360;
                    double abs_angle_error = std::abs(angle_error);

                    // Detect if we're moving away from target (overshoot detection)
                    bool distance_increasing = distance > previous_distance_ &&
                                              previous_distance_ < ARRIVAL_DISTANCE_THRESHOLD * 2;

                    // Update previous distance for next iteration
                    previous_distance_ = distance;

                    // Check for arrival
                    bool arrival_condition = (distance <= ARRIVAL_DISTANCE_THRESHOLD &&
                                             abs_angle_error <= ARRIVAL_ANGLE_THRESHOLD) ||
                                            (distance_increasing && distance < ARRIVAL_DISTANCE_THRESHOLD * 1.5);

                    if (arrival_condition) {
                        // TARGET REACHED: Set arrived flag and stop outputs
                        output_.arrived = true;
                        output_.acceleration = 0;
                        output_.steering = 0;

                        LOG_INFO(NC) << "event" << "arrived"
                                     << "dist" << static_cast<int>(distance)
                                     << "ang_err" << static_cast<int>(abs_angle_error)
                                     << "overshoot" << (distance_increasing ? 1 : 0);

                    } else {
                        // STILL NAVIGATING: Execute control algorithms
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
                // MANUAL MODE or FAULT: Bumpless transfer
                // Reset setpoints to current values to avoid control jumps
                setpoint_.target_position_x = sensor_data.position_x;
                setpoint_.target_position_y = sensor_data.position_y;
                setpoint_.target_angle = sensor_data.angle_x;

                // Controllers output zero and reset arrived status
                output_.acceleration = 0;
                output_.steering = 0;
                output_.arrived = false;
            }
        }

        // Wait until next execution time
        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

int NavigationControl::speed_controller(int current_x, int current_y,
                                        int target_x, int target_y) {
    // Calculate distance to target
    int dx = target_x - current_x;
    int dy = target_y - current_y;
    double distance = std::sqrt(dx*dx + dy*dy);

    // Improved proportional controller with deceleration zone
    int control_output;

    if (distance < DECELERATION_DISTANCE) {
        // Within deceleration zone: reduce gain for smoother approach
        // This prevents overshooting by slowing down earlier
        double reduced_gain = KP_SPEED * (distance / DECELERATION_DISTANCE);
        control_output = static_cast<int>(reduced_gain * distance);
    } else {
        // Far from target: normal proportional control
        control_output = static_cast<int>(KP_SPEED * distance);
    }

    // Clamp to valid range [-100, 100]
    if (control_output > 100) control_output = 100;
    if (control_output < -100) control_output = -100;

    return control_output;
}

int NavigationControl::angle_controller(int current_angle, int target_angle) {
    // Calculate angle error
    int error = target_angle - current_angle;

    // Normalize error to [-180, 180] range
    while (error > 180) error -= 360;
    while (error < -180) error += 360;

    // Simple proportional controller
    int control_output = static_cast<int>(KP_ANGLE * error);

    // Clamp to realistic truck steering limits [-30, 30] degrees
    // Mining trucks have limited steering angles
    if (control_output > 30) control_output = 30;
    if (control_output < -30) control_output = -30;

    return control_output;
}
