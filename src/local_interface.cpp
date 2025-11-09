#include "local_interface.h"
#include <iostream>
#include <iomanip>
#include <chrono>

LocalInterface::LocalInterface(CircularBuffer& buffer, int update_period_ms)
    : buffer_(buffer),
      update_period_ms_(update_period_ms),
      running_(false) {

    // Initialize with safe defaults
    truck_state_.fault = false;
    truck_state_.automatic = false;
    latest_sensor_data_ = {};

    std::cout << "[Local Interface] Initialized (update_period=" << update_period_ms_ << "ms)" << std::endl;
}

LocalInterface::~LocalInterface() {
    stop();
}

void LocalInterface::start() {
    if (running_) {
        return;
    }

    running_ = true;
    task_thread_ = std::thread(&LocalInterface::task_loop, this);

    std::cout << "[Local Interface] Task started" << std::endl;
}

void LocalInterface::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    std::cout << "[Local Interface] Task stopped" << std::endl;
}

void LocalInterface::set_truck_state(const TruckState& state) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    truck_state_ = state;
}

void LocalInterface::set_actuator_output(const ActuatorOutput& output) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    actuator_output_ = output;
}

void LocalInterface::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {
        // Peek at latest sensor data
        SensorData sensor_data = buffer_.peek_latest();

        {
            std::lock_guard<std::mutex> lock(display_mutex_);
            latest_sensor_data_ = sensor_data;
        }

        // Display status
        display_status();

        // Wait until next execution time
        next_execution += std::chrono::milliseconds(update_period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

void LocalInterface::display_status() {
    std::lock_guard<std::mutex> lock(display_mutex_);

    // Clear screen (ANSI escape codes)
    std::cout << "\033[2J\033[1;1H";

    // Display header
    std::cout << "========================================" << std::endl;
    std::cout << "   AUTONOMOUS TRUCK - LOCAL INTERFACE  " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Display truck state
    std::cout << "TRUCK STATE:" << std::endl;
    std::cout << "  Mode:        " << (truck_state_.automatic ? "AUTOMATIC" : "MANUAL") << std::endl;
    std::cout << "  Fault:       " << (truck_state_.fault ? "FAULT DETECTED" : "OK") << std::endl;
    std::cout << std::endl;

    // Display sensor measurements
    std::cout << "SENSOR READINGS:" << std::endl;
    std::cout << "  Position:    (" << std::setw(5) << latest_sensor_data_.position_x
              << ", " << std::setw(5) << latest_sensor_data_.position_y << ")" << std::endl;
    std::cout << "  Heading:     " << std::setw(5) << latest_sensor_data_.angle_x << " degrees" << std::endl;
    std::cout << "  Temperature: " << std::setw(5) << latest_sensor_data_.temperature << " °C";

    // Temperature warning indicator
    if (latest_sensor_data_.temperature > 120) {
        std::cout << "  [CRITICAL]";
    } else if (latest_sensor_data_.temperature > 95) {
        std::cout << "  [WARNING]";
    }
    std::cout << std::endl;

    std::cout << "  Electrical:  " << (latest_sensor_data_.fault_electrical ? "FAULT" : "OK") << std::endl;
    std::cout << "  Hydraulic:   " << (latest_sensor_data_.fault_hydraulic ? "FAULT" : "OK") << std::endl;
    std::cout << std::endl;

    // Display actuator outputs
    std::cout << "ACTUATOR OUTPUTS:" << std::endl;
    std::cout << "  Acceleration: " << std::setw(4) << actuator_output_.acceleration << " %" << std::endl;
    std::cout << "  Steering:     " << std::setw(4) << actuator_output_.steering << " degrees" << std::endl;
    std::cout << std::endl;

    // Display instructions (for Stage 2 integration)
    std::cout << "========================================" << std::endl;
    std::cout << "Commands (Stage 2):" << std::endl;
    std::cout << "  A - Auto mode | M - Manual mode | R - Rearm fault" << std::endl;
    std::cout << "  W/S - Accelerate/Brake | A/D - Steer Left/Right" << std::endl;
    std::cout << "========================================" << std::endl;
}
