#ifndef NAVIGATION_CONTROL_H
#define NAVIGATION_CONTROL_H

#include "circular_buffer.h"
#include "common_types.h"
#include "performance_monitor.h"
#include <thread>
#include <atomic>
#include <mutex>

constexpr int NAVIGATION_CONTROL_THREAD_PRIORITY = 70;
constexpr int HALF_CIRCLE_DEG = 180;
constexpr int FULL_CIRCLE_DEG = 360;
constexpr int NEGATIVE_HALF_CIRCLE_DEG = -180;

constexpr double ARRIVAL_RADIUS_UNITS = 5.0;
constexpr double ALIGNMENT_THRESHOLD_DEG = 5.0;

/**
 * @brief Navigation Control Task
 *
 * Implements control algorithms for:
 * 1. Speed control (velocity)
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
     * @param perf_monitor Pointer to performance monitor (optional)
     */
    NavigationControl(CircularBuffer& buffer, int period_ms = 50, PerformanceMonitor* perf_monitor = nullptr);

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
     * @brief Navigation states
     */
    enum class NavState {
        ROTATING,   // Rotating to align with target
        MOVING,     // Moving straight toward target
        ARRIVED     // Within arrival radius
    };

    /**
     * @brief Calculate angle from current position to target
     */
    int calculate_target_heading(int current_x, int current_y, int target_x, int target_y);

    /**
     * @brief Execute simplified navigation control
     */
    void execute_control(const SensorData& sensor_data);

    CircularBuffer& buffer_;                // Reference to shared buffer
    int period_ms_;                         // Control loop period

    std::atomic<bool> running_;             // Task execution flag
    std::thread task_thread_;               // Task thread

    mutable std::mutex control_mutex_;      // Protects control state
    NavigationSetpoint setpoint_;           // Current setpoint values
    TruckState truck_state_;                // Current truck state
    ActuatorOutput output_;                 // Current control outputs
    NavState nav_state_;                    // Current navigation state

    PerformanceMonitor* perf_monitor_;      // Performance monitoring (optional)

    static constexpr int FIXED_SPEED = 30;
    static constexpr int ROTATION_SPEED = 40;
};

#endif // NAVIGATION_CONTROL_H
