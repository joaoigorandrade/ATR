#include "navigation_control.h"
#include <iostream>
#include <chrono>
#include <cmath>

NavigationControl::NavigationControl(CircularBuffer& buffer, int period_ms)
    : buffer_(buffer),
      period_ms_(period_ms),
      running_(false) {

    // Initialize with safe defaults
    truck_state_.fault = false;
    truck_state_.automatic = false;

    std::cout << "[Navigation Control] Initialized (period: " << period_ms_ << "ms)" << std::endl;
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

    std::cout << "[Navigation Control] Task started" << std::endl;
}

void NavigationControl::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    std::cout << "[Navigation Control] Task stopped" << std::endl;
}

void NavigationControl::set_setpoint(const NavigationSetpoint& setpoint) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    setpoint_ = setpoint;
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
                // AUTOMATIC MODE: Execute control algorithms
                output_.acceleration = speed_controller(
                    sensor_data.position_x, sensor_data.position_y,
                    setpoint_.target_position_x, setpoint_.target_position_y
                );

                output_.steering = angle_controller(
                    sensor_data.angle_x,
                    setpoint_.target_angle
                );

            } else {
                // MANUAL MODE or FAULT: Bumpless transfer
                // Set setpoints to current values to avoid control jumps
                // when switching back to automatic
                setpoint_.target_position_x = sensor_data.position_x;
                setpoint_.target_position_y = sensor_data.position_y;
                setpoint_.target_angle = sensor_data.angle_x;

                // Controllers output zero (no influence)
                output_.acceleration = 0;
                output_.steering = 0;
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

    // Simple proportional controller
    // Acceleration proportional to distance from target
    int control_output = static_cast<int>(KP_SPEED * distance);

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
