#include "route_planning.h"
#include <iostream>
#include <cmath>

RoutePlanning::RoutePlanning() {
    // Initialize with origin as default target
    setpoint_.target_position_x = 0;
    setpoint_.target_position_y = 0;
    setpoint_.target_speed = 0;
    setpoint_.target_angle = 0;

    std::cout << "[Route Planning] Initialized" << std::endl;
}

void RoutePlanning::set_target_waypoint(int x, int y, int speed) {
    std::lock_guard<std::mutex> lock(planning_mutex_);

    setpoint_.target_position_x = x;
    setpoint_.target_position_y = y;
    setpoint_.target_speed = speed;

    std::cout << "[Route Planning] New waypoint: (" << x << ", " << y
              << "), speed=" << speed << "%" << std::endl;
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

    // Normalize to [0, 360) range
    if (angle_deg < 0) {
        angle_deg += 360;
    }

    return angle_deg;
}
