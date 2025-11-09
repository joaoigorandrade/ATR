#include "sensor_processing.h"
#include <chrono>
#include <numeric>
#include <iostream>

SensorProcessing::SensorProcessing(CircularBuffer& buffer, size_t filter_order, int period_ms)
    : buffer_(buffer),
      filter_order_(filter_order),
      period_ms_(period_ms),
      running_(false),
      current_raw_data_{0, 0, 0, 20, false, false} {
    // Initialize with default safe values
}

SensorProcessing::~SensorProcessing() {
    stop();
}

void SensorProcessing::start() {
    if (running_) {
        return;  // Already running
    }

    running_ = true;
    task_thread_ = std::thread(&SensorProcessing::task_loop, this);

    std::cout << "[Sensor Processing] Task started (period: " << period_ms_ << "ms, filter order: " << filter_order_ << ")" << std::endl;
}

void SensorProcessing::stop() {
    if (!running_) {
        return;  // Not running
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    std::cout << "[Sensor Processing] Task stopped" << std::endl;
}

void SensorProcessing::set_raw_data(const RawSensorData& data) {
    std::lock_guard<std::mutex> lock(raw_data_mutex_);
    current_raw_data_ = data;
}

void SensorProcessing::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {
        // Read raw sensor data
        RawSensorData raw_data;
        {
            std::lock_guard<std::mutex> lock(raw_data_mutex_);
            raw_data = current_raw_data_;
        }

        // Apply moving average filter to each sensor reading
        int filtered_x = apply_moving_average(raw_data.position_x, position_x_history_);
        int filtered_y = apply_moving_average(raw_data.position_y, position_y_history_);
        int filtered_angle = apply_moving_average(raw_data.angle_x, angle_x_history_);
        int filtered_temp = apply_moving_average(raw_data.temperature, temperature_history_);

        // Create processed sensor data
        SensorData processed_data;
        processed_data.position_x = filtered_x;
        processed_data.position_y = filtered_y;
        processed_data.angle_x = filtered_angle;
        processed_data.temperature = filtered_temp;
        processed_data.fault_electrical = raw_data.fault_electrical;
        processed_data.fault_hydraulic = raw_data.fault_hydraulic;

        // Get timestamp in milliseconds since epoch
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        processed_data.timestamp = ms.count();

        // Write processed data to circular buffer (Producer operation)
        buffer_.write(processed_data);

        // Wait until next execution time (periodic task)
        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

int SensorProcessing::apply_moving_average(int new_value, std::deque<int>& history) {
    // Add new value to history
    history.push_back(new_value);

    // Maintain window size of filter_order_
    if (history.size() > filter_order_) {
        history.pop_front();
    }

    // Compute average of all values in history
    // Using std::accumulate for clean, readable code
    int sum = std::accumulate(history.begin(), history.end(), 0);
    int average = sum / static_cast<int>(history.size());

    return average;
}
