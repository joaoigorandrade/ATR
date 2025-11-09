#include "data_collector.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

DataCollector::DataCollector(CircularBuffer& buffer, int truck_id, int log_period_ms)
    : buffer_(buffer),
      truck_id_(truck_id),
      log_period_ms_(log_period_ms),
      running_(false) {

    // Initialize state
    current_state_.fault = false;
    current_state_.automatic = false;

    // Generate log filename with truck ID
    std::ostringstream filename;
    filename << "logs/truck_" << truck_id_ << "_log.csv";
    log_filename_ = filename.str();

    std::cout << "[Data Collector] Initialized (truck_id=" << truck_id_
              << ", log_period=" << log_period_ms_ << "ms)" << std::endl;
    std::cout << "[Data Collector] Log file: " << log_filename_ << std::endl;
}

DataCollector::~DataCollector() {
    stop();
}

void DataCollector::start() {
    if (running_) {
        return;
    }

    open_log_file();

    running_ = true;
    task_thread_ = std::thread(&DataCollector::task_loop, this);

    std::cout << "[Data Collector] Task started" << std::endl;
}

void DataCollector::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    close_log_file();

    std::cout << "[Data Collector] Task stopped" << std::endl;
}

void DataCollector::set_truck_state(const TruckState& state) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_state_ = state;
}

void DataCollector::log_event(const EventLog& event) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    if (log_file_.is_open()) {
        // Write CSV line: timestamp, truck_id, state, pos_x, pos_y, description
        log_file_ << event.timestamp << ","
                  << event.truck_id << ","
                  << event.state << ","
                  << event.position_x << ","
                  << event.position_y << ","
                  << event.description << std::endl;
    }
}

void DataCollector::log_event(const std::string& state, int position_x, int position_y,
                              const std::string& description) {
    EventLog event;
    event.timestamp = get_timestamp();
    event.truck_id = truck_id_;
    event.state = state;
    event.position_x = position_x;
    event.position_y = position_y;
    event.description = description;

    log_event(event);
}

void DataCollector::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {
        // Peek at latest sensor data
        SensorData sensor_data = buffer_.peek_latest();

        // Get current state
        TruckState state;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state = current_state_;
        }

        // Build state string
        std::ostringstream state_str;
        if (state.fault) {
            state_str << "FAULT";
        } else if (state.automatic) {
            state_str << "AUTO";
        } else {
            state_str << "MANUAL";
        }

        // Log periodic status
        log_event(state_str.str(),
                 sensor_data.position_x,
                 sensor_data.position_y,
                 "Periodic status update");

        // Wait until next execution time
        next_execution += std::chrono::milliseconds(log_period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

long DataCollector::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return ms.count();
}

void DataCollector::open_log_file() {
    std::lock_guard<std::mutex> lock(log_mutex_);

    log_file_.open(log_filename_, std::ios::out | std::ios::app);

    if (log_file_.is_open()) {
        // Write CSV header if file is new/empty
        log_file_.seekp(0, std::ios::end);
        if (log_file_.tellp() == 0) {
            log_file_ << "Timestamp,TruckID,State,PositionX,PositionY,Description" << std::endl;
        }
    } else {
        std::cerr << "[Data Collector] ERROR: Failed to open log file: " << log_filename_ << std::endl;
    }
}

void DataCollector::close_log_file() {
    std::lock_guard<std::mutex> lock(log_mutex_);

    if (log_file_.is_open()) {
        log_file_.close();
    }
}
