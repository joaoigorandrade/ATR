#ifndef COMMAND_LOGIC_H
#define COMMAND_LOGIC_H

#include "circular_buffer.h"
#include "common_types.h"
#include <thread>
#include <atomic>
#include <mutex>

/**
 * @brief Command Logic Task
 *
 * Central state machine that determines the truck's operational state
 * based on sensor data and operator commands. This task:
 *
 * 1. Reads processed sensor data from circular buffer (Consumer)
 * 2. Monitors fault conditions
 * 3. Processes operator commands (mode switches, rearm)
 * 4. Determines truck state (manual/automatic, fault/ok)
 * 5. Calculates actuator outputs based on current mode
 *
 * State Machine:
 * - Manual Mode: Direct operator control of acceleration/steering
 * - Automatic Mode: Navigation control determines outputs
 * - Fault State: Overrides other states, requires rearm
 *
 * Real-Time Automation Concepts:
 * - State machine implementation
 * - Consumer in Producer-Consumer pattern
 * - Mutex-protected shared state
 * - Event-driven architecture
 */
class CommandLogic {
public:
    /**
     * @brief Construct Command Logic task
     *
     * @param buffer Reference to shared circular buffer
     * @param period_ms Task execution period in milliseconds (default: 50ms)
     */
    CommandLogic(CircularBuffer& buffer, int period_ms = 50);

    /**
     * @brief Destroy Command Logic task and stop thread
     */
    ~CommandLogic();

    /**
     * @brief Start the command logic task thread
     */
    void start();

    /**
     * @brief Stop the command logic task thread
     */
    void stop();

    /**
     * @brief Check if task is running
     */
    bool is_running() const { return running_; }

    /**
     * @brief Set operator command (from Local Interface)
     *
     * Thread-safe method to receive commands from operator.
     *
     * @param cmd Operator command structure
     */
    void set_command(const OperatorCommand& cmd);

    /**
     * @brief Get current truck state
     *
     * Thread-safe read of current operational state.
     *
     * @return TruckState Current state (fault, automatic)
     */
    TruckState get_state() const;

    /**
     * @brief Get current actuator outputs
     *
     * Thread-safe read of actuator values.
     *
     * @return ActuatorOutput Current acceleration and steering
     */
    ActuatorOutput get_actuator_output() const;

    /**
     * @brief Get latest sensor data
     *
     * Returns the most recent sensor reading processed by this task.
     *
     * @return SensorData Latest sensor values
     */
    SensorData get_latest_sensor_data() const;

    /**
     * @brief Set navigation setpoint (from Route Planning/Navigation Control)
     *
     * In automatic mode, these values influence actuator outputs.
     *
     * @param setpoint Navigation setpoint values
     */
    void set_navigation_output(const ActuatorOutput& output);

private:
    /**
     * @brief Main task loop
     */
    void task_loop();

    /**
     * @brief Process operator commands and update state
     */
    void process_commands();

    /**
     * @brief Check for fault conditions in sensor data
     *
     * @param data Sensor data to check
     * @return true if fault detected
     */
    bool check_faults(const SensorData& data);

    /**
     * @brief Calculate actuator outputs based on current mode and commands
     */
    void calculate_actuator_outputs();

    CircularBuffer& buffer_;            // Reference to shared buffer
    int period_ms_;                     // Task period in milliseconds

    std::atomic<bool> running_;         // Task execution flag
    std::thread task_thread_;           // Task thread

    // Protected state variables
    mutable std::mutex state_mutex_;
    TruckState current_state_;          // Current operational state
    ActuatorOutput actuator_output_;    // Current actuator values
    SensorData latest_sensor_data_;     // Latest sensor reading
    OperatorCommand pending_command_;   // Pending operator command
    ActuatorOutput navigation_output_;  // Output from navigation control

    bool command_pending_;              // Flag for new command
    bool fault_rearmed_;                // Flag for fault rearm action
};

#endif // COMMAND_LOGIC_H
