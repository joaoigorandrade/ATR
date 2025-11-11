#include "route_planning.h"
#include "logger.h"
#include <cmath>

RoutePlanning::RoutePlanning() {
    // Initialize with origin as default target
    setpoint_.target_position_x = 0;
    setpoint_.target_position_y = 0;
    setpoint_.target_speed = 0;
    setpoint_.target_angle = 0;

    LOG_INFO(RP) << "event" << "init";
}

void RoutePlanning::set_target_waypoint(int x, int y, int speed) {
    std::lock_guard<std::mutex> lock(planning_mutex_);

    setpoint_.target_position_x = x;
    setpoint_.target_position_y = y;
    setpoint_.target_speed = speed;

    LOG_INFO(RP) << "event" << "waypoint" << "x" << x << "y" << y << "speed" << speed;
}

NavigationSetpoint RoutePlanning::get_setpoint() const {
    std::lock_guard<std::mutex> lock(planning_mutex_);
    return setpoint_;
}

int RoutePlanning::calculate_target_angle(int current_x, int current_y) const {
    std::lock_guard<std::mutex> lock(planning_mutex_);

    int dx = setpoint_.target_position_x - current_x;
    int dy = setpoint_.target_position_y - current_y;

    // Calculate angle in radians, then convert to degrees
    // atan2 returns angle in range [-π, π]
    double angle_rad = std::atan2(dy, dx);
    int angle_deg = static_cast<int>(angle_rad * 180.0 / M_PI);

    return angle_deg;
}
