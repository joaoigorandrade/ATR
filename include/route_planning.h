#ifndef ROUTE_PLANNING_H
#define ROUTE_PLANNING_H

#include "common_types.h"
#include <mutex>
#include <vector>

struct Obstacle {
    int id;
    int x;
    int y;
};

/**
 * @brief Route Planning Task
 *
 * Simplified route planning that sets navigation setpoints.
 * In Stage 1, provides simple waypoint-based navigation.
 * In Stage 2, will integrate with Mine Management for dynamic routing.
 * Now supports dynamic obstacle avoidance (contouring).
 *
 * Real-Time Automation Concepts:
 * - Path planning
 * - Waypoint navigation
 * - Integration with supervisory control (Mine Management)
 * - Collision avoidance
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
     * @brief Update list of known obstacles
     * 
     * @param obstacles Vector of obstacles
     */
    void update_obstacles(const std::vector<Obstacle>& obstacles);

    /**
     * @brief Get current navigation setpoint (raw)
     *
     * @return NavigationSetpoint Current setpoint for navigation
     */
    NavigationSetpoint get_setpoint() const;

    /**
     * @brief Calculate adjusted setpoint avoiding obstacles
     * 
     * @param current_x Current X position
     * @param current_y Current Y position
     * @return NavigationSetpoint Adjusted setpoint (detour if needed)
     */
    NavigationSetpoint calculate_adjusted_setpoint(int current_x, int current_y) const;

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
    std::vector<Obstacle> obstacles_;      // Known obstacles
    
    // Avoidance parameters
    const int AVOIDANCE_RADIUS = 80;       // Safety radius around obstacles
    const int DETECTION_DISTANCE = 200;    // Look-ahead distance
};

#endif // ROUTE_PLANNING_H
