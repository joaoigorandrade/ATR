#include "navigation_control.h"
#include "logger.h"
#include "watchdog.h"
#include <chrono>
#include <cmath>
#include <limits>
#include <pthread.h>
#include <cstring>

NavigationControl::NavigationControl(CircularBuffer& buffer, int period_ms, PerformanceMonitor* perf_monitor)
    : buffer_(buffer),
      period_ms_(period_ms),
      running_(false),
      perf_monitor_(perf_monitor) {

    truck_state_.fault = false;
    truck_state_.automatic = false;
    output_.arrived = false;
    output_.velocity = 0;
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
    pthread_t native_handle = task_thread_.native_handle();
    struct sched_param param;
    param.sched_priority = NAVIGATION_CONTROL_THREAD_PRIORITY;
    int result = pthread_setschedparam(native_handle, SCHED_FIFO, &param);
    if (result == 0) {
        LOG_INFO(NC) << "event" << "start" << "rt_priority" << NAVIGATION_CONTROL_THREAD_PRIORITY << "sched" << "FIFO";
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
        output_.arrived = false;
        LOG_INFO(NC) << "event" << "new_target"
                     << "tgt_x" << setpoint_.target_position_x
                     << "tgt_y" << setpoint_.target_position_y
                     << "old_x" << setpoint.target_position_x
                     << "old_y" << setpoint.target_position_y;
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
        auto start_time = std::chrono::steady_clock::now();

        SensorData sensor_data = buffer_.peek_latest();

        {
            std::lock_guard<std::mutex> lock(control_mutex_);

            bool controllers_enabled = truck_state_.automatic && !truck_state_.fault;

            if (controllers_enabled) {
                execute_control(sensor_data);
            } else {
                setpoint_.target_position_x = sensor_data.position_x;
                setpoint_.target_position_y = sensor_data.position_y;
                setpoint_.target_angle = sensor_data.angle_x;

                output_.velocity = 0;
                output_.steering = sensor_data.angle_x;
                output_.arrived = false;
            }
        }

        if (Watchdog::get_instance()) {
            Watchdog::get_instance()->heartbeat("NavigationControl");
        }

        if (perf_monitor_) {
            perf_monitor_->end_measurement("NavigationControl", start_time);
        }

        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

int NavigationControl::calculate_target_heading(int current_x, int current_y,
                                                 int target_x, int target_y) {
    int dx = target_x - current_x;
    int dy = target_y - current_y;

    double angle_rad = std::atan2(dy, dx);
    int angle_deg = static_cast<int>(angle_rad * HALF_CIRCLE_DEG / M_PI);

    while (angle_deg < 0) angle_deg += FULL_CIRCLE_DEG;
    while (angle_deg >= FULL_CIRCLE_DEG) angle_deg -= FULL_CIRCLE_DEG;

    return angle_deg;
}

int NavigationControl::clamp(int value, int min_val, int max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

void NavigationControl::execute_control(const SensorData& sensor_data) {
    int dx = setpoint_.target_position_x - sensor_data.position_x;
    int dy = setpoint_.target_position_y - sensor_data.position_y;
    double distance = std::sqrt(dx*dx + dy*dy);

    LOG_DEBUG(NC) << "event" << "nav_update"
                  << "dist" << static_cast<int>(distance)
                  << "cur_x" << sensor_data.position_x
                  << "cur_y" << sensor_data.position_y
                  << "tgt_x" << setpoint_.target_position_x
                  << "tgt_y" << setpoint_.target_position_y;

    if (output_.arrived) {
        if (distance > DEPARTURE_THRESHOLD_UNITS) {
            output_.arrived = false;
            LOG_INFO(NC) << "event" << "departure"
                         << "dist" << static_cast<int>(distance);
        } else {
            output_.velocity = 0;
            output_.steering = sensor_data.angle_x;
            LOG_DEBUG(NC) << "event" << "hold_position"
                          << "dist" << static_cast<int>(distance);
            return;
        }
    }

    if (distance <= ARRIVAL_RADIUS_UNITS) {
        output_.arrived = true;
        output_.velocity = 0;
        output_.steering = sensor_data.angle_x;
        LOG_INFO(NC) << "event" << "arrived"
                     << "dist" << static_cast<int>(distance)
                     << "cur_x" << sensor_data.position_x
                     << "cur_y" << sensor_data.position_y
                     << "tgt_x" << setpoint_.target_position_x
                     << "tgt_y" << setpoint_.target_position_y;
        return;
    }

    int target_heading = calculate_target_heading(
        sensor_data.position_x, sensor_data.position_y,
        setpoint_.target_position_x, setpoint_.target_position_y
    );

    int heading_error = target_heading - sensor_data.angle_x;
    while (heading_error > HALF_CIRCLE_DEG) heading_error -= FULL_CIRCLE_DEG;
    while (heading_error < NEGATIVE_HALF_CIRCLE_DEG) heading_error += FULL_CIRCLE_DEG;

    double abs_heading_error = std::abs(heading_error);

    double speed_control = distance * SPEED_GAIN;
    int desired_speed = static_cast<int>(speed_control);

    if (desired_speed > MAX_SPEED) {
        desired_speed = MAX_SPEED;
    } else if (distance > ARRIVAL_RADIUS_UNITS * 3 && desired_speed < MIN_SPEED) {
        desired_speed = MIN_SPEED;
    } else if (desired_speed < 0) {
        desired_speed = 0;
    }

    if (abs_heading_error > HEADING_DEADBAND_DEG) {
        output_.steering = target_heading;
    }

    output_.velocity = desired_speed;

    LOG_DEBUG(NC) << "event" << "p_control"
                  << "dist" << static_cast<int>(distance)
                  << "vel" << output_.velocity
                  << "tgt_hdg" << target_heading
                  << "cur_hdg" << sensor_data.angle_x
                  << "hdg_err" << static_cast<int>(abs_heading_error)
                  << "str" << output_.steering;
}
