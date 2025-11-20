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
      nav_state_(NavState::ROTATING),
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
        nav_state_ = NavState::ROTATING;
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
                output_.steering = 0;
                output_.arrived = false;
                nav_state_ = NavState::ROTATING;
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

void NavigationControl::execute_control(const SensorData& sensor_data) {
    int dx = setpoint_.target_position_x - sensor_data.position_x;
    int dy = setpoint_.target_position_y - sensor_data.position_y;
    double distance = std::sqrt(dx*dx + dy*dy);

    if (distance <= ARRIVAL_RADIUS_UNITS) {
        if (nav_state_ != NavState::ARRIVED) {
            nav_state_ = NavState::ARRIVED;
            output_.arrived = true;
            LOG_INFO(NC) << "event" << "arrived"
                         << "dist" << static_cast<int>(distance);
        }
        output_.velocity = 0;
        output_.steering = 0;
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

    switch (nav_state_) {
        case NavState::ROTATING:
            output_.velocity = 0;

            if (abs_heading_error <= ALIGNMENT_THRESHOLD_DEG) {
                nav_state_ = NavState::MOVING;
                LOG_INFO(NC) << "event" << "aligned"
                             << "heading_err" << static_cast<int>(abs_heading_error);
            } else {
                if (heading_error > 0) {
                    output_.steering = ROTATION_SPEED;
                } else {
                    output_.steering = -ROTATION_SPEED;
                }
            }
            break;

        case NavState::MOVING:
            output_.velocity = FIXED_SPEED;
            output_.steering = 0;

            if (abs_heading_error > ALIGNMENT_THRESHOLD_DEG * 2) {
                nav_state_ = NavState::ROTATING;
                LOG_INFO(NC) << "event" << "misaligned"
                             << "heading_err" << static_cast<int>(abs_heading_error);
            }
            break;

        case NavState::ARRIVED:
            output_.velocity = 0;
            output_.steering = 0;
            break;
    }
}