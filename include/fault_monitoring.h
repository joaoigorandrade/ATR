#ifndef FAULT_MONITORING_H
#define FAULT_MONITORING_H

#include "circular_buffer.h"
#include "common_types.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>

/**
 * @brief Fault event callback type
 *
 * Allows other tasks to register callbacks for fault events
 */
using FaultCallback = std::function<void(FaultType, const SensorData&)>;

/**
 * @brief Fault Monitoring Task
 *
 * Continuously monitors sensor data for fault conditions:
 * - Temperature alert (T > 95°C)
 * - Temperature critical (T > 120°C)
 * - Electrical system faults
 * - Hydraulic system faults
 *
 * When a fault is detected, this task triggers events to:
 * - Command Logic (via shared state)
 * - Navigation Control (to disable control)
 * - Data Collector (for logging)
 *
 * Real-Time Automation Concepts:
 * - Event-driven communication
 * - Condition variables for event notification
 * - Observer pattern (callbacks)
 */
class FaultMonitoring {
public:
    /**
     * @brief Construct Fault Monitoring task
     *
     * @param buffer Reference to shared circular buffer
     * @param period_ms Task execution period in milliseconds (default: 100ms)
     */
    FaultMonitoring(CircularBuffer& buffer, int period_ms = 100);

    /**
     * @brief Destroy Fault Monitoring task
     */
    ~FaultMonitoring();

    /**
     * @brief Start the fault monitoring task
     */
    void start();

    /**
     * @brief Stop the fault monitoring task
     */
    void stop();

    /**
     * @brief Check if task is running
     */
    bool is_running() const { return running_; }

    /**
     * @brief Register a callback for fault events
     *
     * @param callback Function to call when fault detected
     */
    void register_fault_callback(FaultCallback callback);

    /**
     * @brief Get current fault type
     */
    FaultType get_current_fault() const;

private:
    /**
     * @brief Main task loop
     */
    void task_loop();

    /**
     * @brief Check sensor data for faults
     *
     * @param data Sensor data to check
     * @return FaultType Type of fault detected (or NONE)
     */
    FaultType check_for_faults(const SensorData& data);

    /**
     * @brief Notify all registered callbacks of fault event
     *
     * @param fault_type Type of fault detected
     * @param data Sensor data at time of fault
     */
    void notify_fault_event(FaultType fault_type, const SensorData& data);

    CircularBuffer& buffer_;                // Reference to shared buffer
    int period_ms_;                         // Task period

    std::atomic<bool> running_;             // Task execution flag
    std::thread task_thread_;               // Task thread

    mutable std::mutex fault_mutex_;        // Protects fault state
    FaultType current_fault_;               // Current active fault

    std::mutex callback_mutex_;             // Protects callback list
    std::vector<FaultCallback> callbacks_;  // Registered fault callbacks
};

#endif // FAULT_MONITORING_H
