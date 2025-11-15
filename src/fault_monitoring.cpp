#include "fault_monitoring.h"
#include "logger.h"
#include "watchdog.h"
#include <chrono>
#include <pthread.h>
#include <cstring>

FaultMonitoring::FaultMonitoring(CircularBuffer& buffer, int period_ms, PerformanceMonitor* perf_monitor)
    : buffer_(buffer),
      period_ms_(period_ms),
      running_(false),
      current_fault_(FaultType::NONE),
      perf_monitor_(perf_monitor) {

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

    // Set real-time scheduling priority (highest priority for fault monitoring)
    // SCHED_FIFO ensures deterministic scheduling for hard real-time requirements
    pthread_t native_handle = task_thread_.native_handle();
    struct sched_param param;
    param.sched_priority = 90; // Priority range: 1-99 (90 = highest for this system)

    int result = pthread_setschedparam(native_handle, SCHED_FIFO, &param);
    if (result == 0) {
        LOG_INFO(FM) << "event" << "start" << "rt_priority" << 90 << "sched" << "FIFO";
    } else {
        // Failed to set real-time priority (might need sudo/capabilities)
        LOG_WARN(FM) << "event" << "start" << "rt_priority" << "failed" << "errno" << result;
    }
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
        auto start_time = std::chrono::steady_clock::now();

        SensorData sensor_data = buffer_.peek_latest();
        FaultType fault = check_for_faults(sensor_data);

        {
            std::lock_guard<std::mutex> lock(fault_mutex_);
            if (fault != current_fault_) {
                current_fault_ = fault;
                if (fault != FaultType::NONE) {
                    notify_fault_event(fault, sensor_data);
                }
            }
        }

        // Report heartbeat to watchdog
        if (Watchdog::get_instance()) {
            Watchdog::get_instance()->heartbeat("FaultMonitoring");
        }

        // Record execution time
        if (perf_monitor_) {
            perf_monitor_->end_measurement("FaultMonitoring", start_time);
        }

        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

FaultType FaultMonitoring::check_for_faults(const SensorData& data) {
    if (data.temperature > 120) {
        return FaultType::TEMPERATURE_CRITICAL;
    }
    if (data.fault_electrical) {
        return FaultType::ELECTRICAL;
    }
    if (data.fault_hydraulic) {
        return FaultType::HYDRAULIC;
    }
    if (data.temperature > 95) {
        return FaultType::TEMPERATURE_ALERT;
    }

    return FaultType::NONE;
}

void FaultMonitoring::notify_fault_event(FaultType fault_type, const SensorData& data) {
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

    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (const auto& callback : callbacks_) {
        callback(fault_type, data);
    }
}
