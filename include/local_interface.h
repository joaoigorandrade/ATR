#ifndef LOCAL_INTERFACE_H
#define LOCAL_INTERFACE_H

#include "circular_buffer.h"
#include "common_types.h"
#include <thread>
#include <atomic>
#include <mutex>

/**
 * @brief Local Interface Task
 *
 * Handles interaction with the local operator (inside the truck).
 *
 * Responsibilities:
 * 1. Display system states and measurements on terminal
 * 2. Accept operator commands via keyboard (Stage 1: simplified)
 * 3. Send commands to Command Logic task
 *
 * In Stage 1, this provides a simple terminal display.
 * Full keyboard interaction will be integrated with all tasks.
 *
 * Real-Time Automation Concepts:
 * - Human-machine interface (HMI)
 * - Operator interaction in real-time systems
 * - Status monitoring and display
 */
class LocalInterface {
public:
    /**
     * @brief Construct Local Interface task
     *
     * @param buffer Reference to shared circular buffer
     * @param update_period_ms Display update period (default: 1000ms)
     */
    LocalInterface(CircularBuffer& buffer, int update_period_ms = 1000);

    /**
     * @brief Destroy Local Interface task
     */
    ~LocalInterface();

    /**
     * @brief Start the local interface task
     */
    void start();

    /**
     * @brief Stop the local interface task
     */
    void stop();

    /**
     * @brief Check if task is running
     */
    bool is_running() const { return running_; }

    /**
     * @brief Set truck state (from Command Logic)
     *
     * @param state Current truck state
     */
    void set_truck_state(const TruckState& state);

    /**
     * @brief Set actuator output (from Command Logic/Navigation)
     *
     * @param output Current actuator values
     */
    void set_actuator_output(const ActuatorOutput& output);

private:
    /**
     * @brief Main display loop
     */
    void task_loop();

    /**
     * @brief Display current truck status
     */
    void display_status();

    CircularBuffer& buffer_;                // Reference to shared buffer
    int update_period_ms_;                  // Display update period

    std::atomic<bool> running_;             // Task execution flag
    std::thread task_thread_;               // Task thread

    mutable std::mutex display_mutex_;      // Protects display data
    TruckState truck_state_;                // Current truck state
    ActuatorOutput actuator_output_;        // Current actuator values
    SensorData latest_sensor_data_;         // Latest sensor readings
    size_t buffer_count_;                   // Debug: buffer count
};

#endif // LOCAL_INTERFACE_H
