#ifndef SENSOR_PROCESSING_H
#define SENSOR_PROCESSING_H

#include "circular_buffer.h"
#include "performance_monitor.h"
#include <thread>
#include <atomic>
#include <deque>

constexpr int SENSOR_PROCESSING_THREAD_PRIORITY = 60;

/**
 * @brief Raw sensor readings from the truck's sensors
 *
 * This structure contains unfiltered sensor data that will be
 * processed by the Sensor Processing task.
 */
struct RawSensorData {
    int position_x;         // Raw position X (with noise)
    int position_y;         // Raw position Y (with noise)
    int angle_x;            // Raw angle measurement
    int temperature;        // Engine temperature
    bool fault_electrical;  // Electrical fault flag
    bool fault_hydraulic;   // Hydraulic fault flag
};

/**
 * @brief Sensor Processing Task
 *
 * Responsible for:
 * 1. Reading raw sensor data from hardware (simulated for Stage 1)
 * 2. Applying moving average filter to reduce noise
 * 3. Writing processed data to circular buffer
 *
 * This is a periodic task that runs at a fixed interval (e.g., 100ms).
 * Implements noise filtering using a moving average of order M,
 * which is essential in real-time systems to obtain reliable measurements.
 *
 * Real-Time Automation Concepts:
 * - Periodic task execution
 * - Digital signal processing (moving average filter)
 * - Producer role in Producer-Consumer pattern
 */
class SensorProcessing {
public:
    /**
     * @brief Construct Sensor Processing task
     *
     * @param buffer Reference to shared circular buffer
     * @param filter_order Order M of moving average filter (default: 5)
     * @param period_ms Task execution period in milliseconds (default: 100ms)
     * @param perf_monitor Pointer to performance monitor (optional)
     */
    SensorProcessing(CircularBuffer& buffer, size_t filter_order = 5, int period_ms = 100,
                     PerformanceMonitor* perf_monitor = nullptr);

    /**
     * @brief Destroy Sensor Processing task and stop thread
     */
    ~SensorProcessing();

    /**
     * @brief Start the sensor processing task thread
     */
    void start();

    /**
     * @brief Stop the sensor processing task thread
     */
    void stop();

    /**
     * @brief Check if task is running
     *
     * @return true if task thread is active
     */
    bool is_running() const { return running_; }

    /**
     * @brief Set raw sensor data (used for simulation in Stage 1)
     *
     * In Stage 2, this will be replaced by MQTT subscription or
     * direct hardware interface.
     *
     * @param data Raw sensor readings
     */
    void set_raw_data(const RawSensorData& data);

private:
    /**
     * @brief Main task loop executed by the thread
     *
     * Periodically reads sensors, applies filter, and writes to buffer
     */
    void task_loop();

    /**
     * @brief Apply moving average filter to position data
     *
     * Maintains a window of M samples and computes the average.
     * This reduces high-frequency noise while preserving signal trends.
     *
     * @param new_value New raw sensor value
     * @param history Deque containing recent samples
     * @return int Filtered value (average of last M samples)
     */
    int apply_moving_average(int new_value, std::deque<int>& history);

    CircularBuffer& buffer_;            // Reference to shared buffer
    size_t filter_order_;               // Number of samples for moving average
    int period_ms_;                     // Task period in milliseconds

    std::atomic<bool> running_;         // Flag to control task execution
    std::thread task_thread_;           // Thread executing the task

    PerformanceMonitor* perf_monitor_;  // Performance monitoring (optional)

    // Moving average history for each filtered sensor
    std::deque<int> position_x_history_;
    std::deque<int> position_y_history_;
    std::deque<int> angle_x_history_;
    std::deque<int> temperature_history_;

    // Current raw sensor data (simulated for Stage 1)
    RawSensorData current_raw_data_;
    std::mutex raw_data_mutex_;         // Protect access to raw data
};

#endif // SENSOR_PROCESSING_H
