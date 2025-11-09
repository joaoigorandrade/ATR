#ifndef ROUTE_PLANNING_H
#define ROUTE_PLANNING_H

#include "common_types.h"
#include <mutex>

/**
 * @brief Route Planning Task
 *
 * Simplified route planning that sets navigation setpoints.
 * In Stage 1, provides simple waypoint-based navigation.
 * In Stage 2, will integrate with Mine Management for dynamic routing.
 *
 * Real-Time Automation Concepts:
 * - Path planning
 * - Waypoint navigation
 * - Integration with supervisory control (Mine Management)
 */
class RoutePlanning {
public:
    /**
     * @brief Construct Route Planning task
     */
    RoutePlanning();

    /**
     * @brief Set target waypoint
     *
     * @param x Target X coordinate
     * @param y Target Y coordinate
     * @param speed Target speed (percentage)
     */
    void set_target_waypoint(int x, int y, int speed);

    /**
     * @brief Get current navigation setpoint
     *
     * @return NavigationSetpoint Current setpoint for navigation
     */
    NavigationSetpoint get_setpoint() const;

    /**
     * @brief Calculate heading angle to target
     *
     * @param current_x Current X position
     * @param current_y Current Y position
     * @return int Target heading angle (degrees)
     */
    int calculate_target_angle(int current_x, int current_y) const;

private:
    mutable std::mutex planning_mutex_;    // Protects planning state
    NavigationSetpoint setpoint_;          // Current navigation setpoint
};

#endif // ROUTE_PLANNING_H
