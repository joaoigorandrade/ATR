#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include "circular_buffer.h"
#include "common_types.h"
#include "performance_monitor.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <string>

/**
 * @brief Event log entry structure
 */
struct EventLog {
    long timestamp;            // Timestamp in milliseconds
    int truck_id;              // Truck identification number
    std::string state;         // Truck state description
    int position_x;            // X coordinate
    int position_y;            // Y coordinate
    std::string description;   // Event description
};

/**
 * @brief Data Collector Task
 *
 * Responsible for:
 * 1. Collecting system state data
 * 2. Assigning timestamps to events
 * 3. Logging events to disk in structured format
 * 4. Organizing data for Local Interface
 *
 * Logs are stored in CSV format for easy analysis.
 *
 * Real-Time Automation Concepts:
 * - Data logging and persistence
 * - File I/O with mutex protection
 * - System monitoring and diagnostics
 */
class DataCollector {
public:
    /**
     * @brief Construct Data Collector task
     *
     * @param buffer Reference to shared circular buffer
     * @param truck_id Truck identification number
     * @param log_period_ms Logging period in milliseconds (default: 1000ms)
     * @param perf_monitor Pointer to performance monitor (optional)
     */
    DataCollector(CircularBuffer& buffer, int truck_id = 1, int log_period_ms = 1000, PerformanceMonitor* perf_monitor = nullptr);

    /**
     * @brief Destroy Data Collector task
     */
    ~DataCollector();

    /**
     * @brief Start the data collector task
     */
    void start();

    /**
     * @brief Stop the data collector task
     */
    void stop();

    /**
     * @brief Check if task is running
     */
    bool is_running() const { return running_; }

    /**
     * @brief Log an event to disk
     *
     * @param event Event log entry to write
     */
    void log_event(const EventLog& event);

    /**
     * @brief Log an event with description
     *
     * @param state Truck state description
     * @param position_x X coordinate
     * @param position_y Y coordinate
     * @param description Event description
     */
    void log_event(const std::string& state, int position_x, int position_y,
                   const std::string& description);

    /**
     * @brief Set current truck state (from Command Logic)
     *
     * @param state Current truck state
     */
    void set_truck_state(const TruckState& state);

private:
    /**
     * @brief Main task loop
     */
    void task_loop();

    /**
     * @brief Get current timestamp in milliseconds
     *
     * @return long Timestamp in milliseconds since epoch
     */
    long get_timestamp() const;

    /**
     * @brief Open log file for writing
     */
    void open_log_file();

    /**
     * @brief Close log file
     */
    void close_log_file();

    CircularBuffer& buffer_;                // Reference to shared buffer
    int truck_id_;                          // Truck ID
    int log_period_ms_;                     // Logging period

    std::atomic<bool> running_;             // Task execution flag
    std::thread task_thread_;               // Task thread

    mutable std::mutex log_mutex_;          // Protects log file access
    std::ofstream log_file_;                // Log file stream
    std::string log_filename_;              // Log file name

    mutable std::mutex state_mutex_;        // Protects state data
    TruckState current_state_;              // Current truck state

    PerformanceMonitor* perf_monitor_;      // Performance monitoring (optional)
};

#endif // DATA_COLLECTOR_H
