#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <chrono>
#include <string>
#include <mutex>
#include <map>
#include <vector>

/**
 * @brief Performance monitoring for real-time tasks
 *
 * Tracks execution time statistics for each task:
 * - Current, min, max, average execution time
 * - Standard deviation and jitter
 * - Deadline violation detection
 * - Statistical analysis over sliding window
 *
 * Real-Time Automation Concepts:
 * - WCET (Worst-Case Execution Time) monitoring
 * - Deadline miss detection
 * - Jitter analysis (execution time variability)
 * - Performance profiling
 */

class PerformanceMonitor {
public:
    struct TaskStats {
        std::string task_name;
        int expected_period_ms;        // Expected task period

        // Execution time statistics (microseconds)
        long current_execution_us;     // Last execution time
        long min_execution_us;         // Minimum observed
        long max_execution_us;         // Maximum observed (WCET estimate)
        double avg_execution_us;       // Average execution time
        double std_dev_us;             // Standard deviation (jitter)

        // Deadline monitoring
        int deadline_violations;       // Count of deadline misses
        long worst_overrun_us;        // Worst deadline overrun

        // Sample tracking
        int sample_count;              // Number of measurements
        std::vector<long> recent_samples; // Sliding window (last 100)

        TaskStats()
            : expected_period_ms(0)
            , current_execution_us(0)
            , min_execution_us(LONG_MAX)
            , max_execution_us(0)
            , avg_execution_us(0.0)
            , std_dev_us(0.0)
            , deadline_violations(0)
            , worst_overrun_us(0)
            , sample_count(0) {
            recent_samples.reserve(100);
        }
    };

    /**
     * @brief Register a task for monitoring
     * @param task_name Unique task identifier
     * @param expected_period_ms Expected period in milliseconds (for deadline detection)
     */
    void register_task(const std::string& task_name, int expected_period_ms);

    /**
     * @brief Start timing a task execution
     * @param task_name Task identifier
     * @return Start timepoint (pass to end_measurement)
     */
    std::chrono::steady_clock::time_point start_measurement(const std::string& task_name);

    /**
     * @brief End timing and update statistics
     * @param task_name Task identifier
     * @param start_time Timepoint from start_measurement
     */
    void end_measurement(const std::string& task_name,
                        std::chrono::steady_clock::time_point start_time);

    /**
     * @brief Get statistics for a specific task
     * @param task_name Task identifier
     * @return Task statistics (copy)
     */
    TaskStats get_stats(const std::string& task_name) const;

    /**
     * @brief Get all task statistics
     * @return Map of all task stats
     */
    std::map<std::string, TaskStats> get_all_stats() const;

    /**
     * @brief Reset statistics for a task
     * @param task_name Task identifier
     */
    void reset_stats(const std::string& task_name);

    /**
     * @brief Reset all statistics
     */
    void reset_all_stats();

    /**
     * @brief Print performance report to console
     */
    void print_report() const;

    /**
     * @brief Get formatted performance summary
     * @return Multi-line string with performance data
     */
    std::string get_report_string() const;

    /**
     * @brief Check if any task has deadline violations
     * @return True if violations detected
     */
    bool has_deadline_violations() const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, TaskStats> task_stats_;

    // Helper methods
    void update_statistics(TaskStats& stats, long execution_us);
    double calculate_std_dev(const std::vector<long>& samples, double mean) const;
};

/**
 * @brief RAII helper for automatic timing
 *
 * Usage:
 *   void task_loop() {
 *       while(running_) {
 *           PerformanceMeasurement pm(perf_monitor, "TaskName");
 *           // ... do work ...
 *       } // pm destructor automatically records time
 *   }
 */
class PerformanceMeasurement {
public:
    PerformanceMeasurement(PerformanceMonitor& monitor, const std::string& task_name)
        : monitor_(monitor)
        , task_name_(task_name)
        , start_time_(monitor.start_measurement(task_name)) {}

    ~PerformanceMeasurement() {
        monitor_.end_measurement(task_name_, start_time_);
    }

    // Prevent copying
    PerformanceMeasurement(const PerformanceMeasurement&) = delete;
    PerformanceMeasurement& operator=(const PerformanceMeasurement&) = delete;

private:
    PerformanceMonitor& monitor_;
    std::string task_name_;
    std::chrono::steady_clock::time_point start_time_;
};

#endif // PERFORMANCE_MONITOR_H
