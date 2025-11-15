#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <map>
#include <functional>

/**
 * @brief Watchdog Timer for Hard Real-Time Systems
 *
 * Monitors critical tasks to detect hangs, crashes, or missed deadlines.
 * Implements a heartbeat mechanism where tasks must periodically report
 * their status. If a task fails to report within its timeout period,
 * the watchdog triggers a fault handler.
 *
 * Real-Time Automation Concepts:
 * - Fault tolerance and detection
 * - Heartbeat/keepalive protocol
 * - Automatic recovery mechanisms
 * - Safety-critical system monitoring
 *
 * Design Pattern:
 * - Observer pattern (tasks report to watchdog)
 * - Fail-safe design (assumes failure if no heartbeat)
 * - Configurable per-task timeouts
 *
 * Usage Example:
 * ```cpp
 * Watchdog watchdog;
 * watchdog.register_task("FaultMonitoring", 200);  // 200ms timeout
 * watchdog.start();
 *
 * // In task loop:
 * watchdog.heartbeat("FaultMonitoring");
 * ```
 */
class Watchdog {
public:
    /**
     * @brief Fault handler callback type
     *
     * Called when a task fails to report within timeout.
     * Parameters: task_name, time_since_last_heartbeat_ms
     */
    using FaultHandler = std::function<void(const std::string&, long)>;

    /**
     * @brief Construct watchdog timer
     *
     * @param check_period_ms How often to check task status (default: 100ms)
     */
    Watchdog(int check_period_ms = 100);

    /**
     * @brief Destroy watchdog and stop monitoring
     */
    ~Watchdog();

    /**
     * @brief Start watchdog monitoring
     */
    void start();

    /**
     * @brief Stop watchdog monitoring
     */
    void stop();

    /**
     * @brief Check if watchdog is running
     */
    bool is_running() const { return running_; }

    /**
     * @brief Register a task for monitoring
     *
     * @param task_name Unique task identifier
     * @param timeout_ms Maximum time between heartbeats (ms)
     */
    void register_task(const std::string& task_name, int timeout_ms);

    /**
     * @brief Unregister a task from monitoring
     *
     * @param task_name Task to remove
     */
    void unregister_task(const std::string& task_name);

    /**
     * @brief Report heartbeat from task (task is alive)
     *
     * Tasks must call this periodically to avoid timeout.
     *
     * @param task_name Task identifier
     */
    void heartbeat(const std::string& task_name);

    /**
     * @brief Set custom fault handler
     *
     * Default handler logs error. Custom handler can implement
     * recovery actions like restarting tasks or shutting down.
     *
     * @param handler Callback function for fault events
     */
    void set_fault_handler(FaultHandler handler);

    /**
     * @brief Get number of registered tasks
     */
    size_t get_task_count() const;

    /**
     * @brief Get total number of faults detected
     */
    int get_fault_count() const { return fault_count_; }

    /**
     * @brief Set global watchdog instance for easy task access
     *
     * @param instance Pointer to watchdog instance
     */
    static void set_instance(Watchdog* instance);

    /**
     * @brief Get global watchdog instance
     *
     * @return Watchdog* Pointer to instance (nullptr if not set)
     */
    static Watchdog* get_instance();

private:
    /**
     * @brief Internal structure for task monitoring
     */
    struct TaskInfo {
        int timeout_ms;                                    // Maximum time between heartbeats
        std::chrono::steady_clock::time_point last_heartbeat;  // Last heartbeat time
        bool ever_reported;                                // Has task ever reported?
        int consecutive_failures;                          // Consecutive timeout count
    };

    /**
     * @brief Main watchdog monitoring loop
     */
    void watchdog_loop();

    /**
     * @brief Check if task has timed out
     *
     * @param task_name Task to check
     * @param info Task information
     * @return true if timed out
     */
    bool is_timed_out(const std::string& task_name, const TaskInfo& info);

    /**
     * @brief Default fault handler (logs error)
     *
     * @param task_name Task that timed out
     * @param elapsed_ms Time since last heartbeat
     */
    void default_fault_handler(const std::string& task_name, long elapsed_ms);

    int check_period_ms_;                              // Watchdog check period
    std::atomic<bool> running_;                        // Watchdog running flag
    std::thread watchdog_thread_;                      // Watchdog thread

    mutable std::mutex tasks_mutex_;                   // Protects task map
    std::map<std::string, TaskInfo> monitored_tasks_;  // Registered tasks

    FaultHandler fault_handler_;                       // Fault callback
    std::atomic<int> fault_count_;                     // Total faults detected
};

#endif // WATCHDOG_H
