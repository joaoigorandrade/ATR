#include "fault_monitoring.h"
#include "logger.h"
#include <chrono>

FaultMonitoring::FaultMonitoring(CircularBuffer& buffer, int period_ms)
    : buffer_(buffer),
      period_ms_(period_ms),
      running_(false),
      current_fault_(FaultType::NONE) {

    LOG_INFO(FM) << "event" << "init" << "period_ms" << period_ms_;
}

FaultMonitoring::~FaultMonitoring() {
    stop();
}

void FaultMonitoring::start() {
    if (running_) {
        return;
    }

    running_ = true;
    task_thread_ = std::thread(&FaultMonitoring::task_loop, this);

    LOG_INFO(FM) << "event" << "start";
}

void FaultMonitoring::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    LOG_INFO(FM) << "event" << "stop";
}

void FaultMonitoring::register_fault_callback(FaultCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callbacks_.push_back(callback);
}

FaultType FaultMonitoring::get_current_fault() const {
    std::lock_guard<std::mutex> lock(fault_mutex_);
    return current_fault_;
}

void FaultMonitoring::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {
        // Peek at latest sensor data (non-consuming read)
        SensorData sensor_data = buffer_.peek_latest();

        // Check for faults
        FaultType fault = check_for_faults(sensor_data);

        {
            std::lock_guard<std::mutex> lock(fault_mutex_);

            // Check if fault state changed
            if (fault != current_fault_) {
                current_fault_ = fault;

                // Notify callbacks of fault event
                if (fault != FaultType::NONE) {
                    notify_fault_event(fault, sensor_data);
                }
            }
        }

        // Wait until next execution time
        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

FaultType FaultMonitoring::check_for_faults(const SensorData& data) {
    // Priority order: most critical first

    // Check critical temperature (T > 120°C)
    if (data.temperature > 120) {
        return FaultType::TEMPERATURE_CRITICAL;
    }

    // Check electrical fault
    if (data.fault_electrical) {
        return FaultType::ELECTRICAL;
    }

    // Check hydraulic fault
    if (data.fault_hydraulic) {
        return FaultType::HYDRAULIC;
    }

    // Check temperature alert (T > 95°C)
    if (data.temperature > 95) {
        return FaultType::TEMPERATURE_ALERT;
    }

    return FaultType::NONE;
}

void FaultMonitoring::notify_fault_event(FaultType fault_type, const SensorData& data) {
    // Log fault event with structured data
    const char* fault_code = "UNK";
    Logger::Level log_level = Logger::Level::WARN;

    switch (fault_type) {
        case FaultType::TEMPERATURE_ALERT:
            fault_code = "TEMP_WRN";
            log_level = Logger::Level::WARN;
            break;
        case FaultType::TEMPERATURE_CRITICAL:
            fault_code = "TEMP_CRT";
            log_level = Logger::Level::CRIT;
            break;
        case FaultType::ELECTRICAL:
            fault_code = "ELEC";
            log_level = Logger::Level::CRIT;
            break;
        case FaultType::HYDRAULIC:
            fault_code = "HYDR";
            log_level = Logger::Level::CRIT;
            break;
        default:
            break;
    }

    Logger::log(log_level, Logger::Module::FM)
        << "event" << "fault"
        << "type" << fault_code
        << "temp" << data.temperature
        << "pos_x" << data.position_x
        << "pos_y" << data.position_y;

    // Notify all registered callbacks
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (const auto& callback : callbacks_) {
        callback(fault_type, data);
    }
}
