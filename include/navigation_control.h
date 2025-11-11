#ifndef NAVIGATION_CONTROL_H
#define NAVIGATION_CONTROL_H

#include "circular_buffer.h"
#include "common_types.h"
#include <thread>
#include <atomic>
#include <mutex>

/**
 * @brief Navigation Control Task
 *
 * Implements control algorithms for:
 * 1. Speed control (acceleration)
 * 2. Angular position control (steering)
 *
 * Operates in two modes:
 * - Manual Mode: Controllers disabled, setpoints track current values (bumpless transfer)
 * - Automatic Mode: Controllers active, following setpoints from Route Planning
 *
 * Uses simple Proportional (P) controllers for demonstration.
 * In production, PID controllers would be used.
 *
 * Real-Time Automation Concepts:
 * - Control systems (feedback loops)
 * - Bumpless transfer between modes
 * - Periodic control task execution
 */
class NavigationControl {
public:
    /**
     * @brief Construct Navigation Control task
     *
     * @param buffer Reference to shared circular buffer
     * @param period_ms Control loop period in milliseconds (default: 50ms)
     */
    NavigationControl(CircularBuffer& buffer, int period_ms = 50);

    /**
     * @brief Destroy Navigation Control task
     */
    ~NavigationControl();

    /**
     * @brief Start the navigation control task
     */
    void start();

    /**
     * @brief Stop the navigation control task
     */
    void stop();

    /**
     * @brief Check if task is running
     */
    bool is_running() const { return running_; }

    /**
     * @brief Set navigation setpoint (from Route Planning)
     *
     * @param setpoint Target values for navigation
     */
    void set_setpoint(const NavigationSetpoint& setpoint);

    /**
     * @brief Set truck state (from Command Logic)
     *
     * Used to determine if controllers should be active.
     *
     * @param state Current truck operational state
     */
    void set_truck_state(const TruckState& state);

    /**
     * @brief Get current control output
     *
     * @return ActuatorOutput Current controller outputs
     */
    ActuatorOutput get_output() const;

private:
    /**
     * @brief Main control loop
     */
    void task_loop();

    /**
     * @brief Execute speed controller
     *
     * Simple proportional controller for speed/acceleration.
     *
     * @param current Current position/speed
     * @param target Target position/speed
     * @return int Control output (acceleration)
     */
    int speed_controller(int current_x, int current_y, int target_x, int target_y);

    /**
     * @brief Execute angular position controller
     *
     * Simple proportional controller for heading angle.
     *
     * @param current_angle Current heading (degrees)
     * @param target_angle Target heading (degrees)
     * @return int Control output (steering angle)
     */
    int angle_controller(int current_angle, int target_angle);

    CircularBuffer& buffer_;                // Reference to shared buffer
    int period_ms_;                         // Control loop period

    std::atomic<bool> running_;             // Task execution flag
    std::thread task_thread_;               // Task thread

    mutable std::mutex control_mutex_;      // Protects control state
    NavigationSetpoint setpoint_;           // Current setpoint values
    TruckState truck_state_;                // Current truck state
    ActuatorOutput output_;                 // Current control outputs
    double previous_distance_;              // Track previous distance for approach detection

    // Control gains (tunable parameters)
    static constexpr double KP_SPEED = 0.6;   // Speed controller gain
    static constexpr double KP_ANGLE = 1.0;   // Angle controller gain

    // Arrival detection thresholds
    static constexpr double ARRIVAL_DISTANCE_THRESHOLD = 4.0;  // Distance in units (increased for reliability)
    static constexpr double ARRIVAL_ANGLE_THRESHOLD = 10.0;     // Angle error in degrees (relaxed)
    static constexpr double DECELERATION_DISTANCE = 80.0;      // Start slowing down at this distance
};

#endif // NAVIGATION_CONTROL_H
