#include "route_planning.h"
#include "logger.h"
#include <cmath>
#include <limits>
#include <algorithm>

RoutePlanning::RoutePlanning() {

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

void RoutePlanning::update_obstacles(const std::vector<Obstacle>& obstacles) {
    std::lock_guard<std::mutex> lock(planning_mutex_);
    obstacles_ = obstacles;
}

NavigationSetpoint RoutePlanning::get_setpoint() const {
    std::lock_guard<std::mutex> lock(planning_mutex_);
    return setpoint_;
}

NavigationSetpoint RoutePlanning::calculate_adjusted_setpoint(int current_x, int current_y) const {
    std::lock_guard<std::mutex> lock(planning_mutex_);
    
    NavigationSetpoint result = setpoint_;
    
    // Vector from current to target
    double dx = setpoint_.target_position_x - current_x;
    double dy = setpoint_.target_position_y - current_y;
    double dist_to_target = std::sqrt(dx*dx + dy*dy);
    
    if (dist_to_target < 1.0) return result; // Already there
    
    // Normalize direction
    double dir_x = dx / dist_to_target;
    double dir_y = dy / dist_to_target;
    
    const Obstacle* closest_threat = nullptr;
    double min_dist_sq = std::numeric_limits<double>::max();
    
    for (const auto& obs : obstacles_) {
        // Vector from current to obstacle
        double ox = obs.x - current_x;
        double oy = obs.y - current_y;
        
        // Project onto path
        double projection = ox * dir_x + oy * dir_y;
        
        // Is it in front of us and within detection range?
        if (projection > 0 && projection < std::min(dist_to_target, (double)DETECTION_DISTANCE)) {
            // Perpendicular distance
            // Cross product (2D) gives signed area, divide by base (1.0) gives height
            double perp_dist = std::abs(ox * dir_y - oy * dir_x);
            
            if (perp_dist < AVOIDANCE_RADIUS) {
                double dist_sq = ox*ox + oy*oy;
                if (dist_sq < min_dist_sq) {
                    min_dist_sq = dist_sq;
                    closest_threat = &obs;
                }
            }
        }
    }
    
    if (closest_threat) {
        // Collision detected! Calculate contour point.
        
        // Vector Current->Obstacle (recalc)
        double ox = closest_threat->x - current_x;
        double oy = closest_threat->y - current_y;
        
        // Cross product to determine side: (Path) x (Obstacle)
        // Path vector (dx, dy). Obstacle vector (ox, oy).
        double cross = dx * oy - dy * ox;
        
        double offset_dir_x, offset_dir_y;
        
        // If cross > 0, obstacle is to the LEFT. We should go RIGHT.
        // If cross < 0, obstacle is to the RIGHT. We should go LEFT.
        if (cross > 0) {
            // Go right: (dir_y, -dir_x)
            offset_dir_x = dir_y;
            offset_dir_y = -dir_x;
        } else {
            // Go left: (-dir_y, dir_x)
            offset_dir_x = -dir_y;
            offset_dir_y = dir_x;
        }
        
        double margin = 20.0;
        double avoid_dist = AVOIDANCE_RADIUS + margin;
        
        // Target a point "beside" the obstacle
        result.target_position_x = closest_threat->x + (int)(offset_dir_x * avoid_dist);
        result.target_position_y = closest_threat->y + (int)(offset_dir_y * avoid_dist);
        
        // Keep same speed
    }
    
    return result;
}

int RoutePlanning::calculate_target_angle(int current_x, int current_y) const {
    std::lock_guard<std::mutex> lock(planning_mutex_);

    // Note: This should use the ADJUSTED setpoint if we want the angle to reflect avoidance.
    // However, this function is const and calls get_setpoint() internally or uses setpoint_.
    // But since calculate_adjusted_setpoint returns a COPY of a setpoint, we can't easily reuse it here
    // unless we change the API or pass the adjusted setpoint in.
    //
    // In main.cpp, we are doing:
    // NavigationSetpoint setpoint = route_planner.get_setpoint(); (NOW calculate_adjusted_setpoint)
    // setpoint.target_angle = route_planner.calculate_target_angle(...);
    //
    // We should update calculate_target_angle to take a target_x/y or just do the math in main.
    // Actually, let's just keep this simple raw calc and let main handle the "real" target.

    int dx = setpoint_.target_position_x - current_x;
    int dy = setpoint_.target_position_y - current_y;

    double angle_rad = std::atan2(dy, dx);
    int angle_deg = static_cast<int>(angle_rad * 180.0 / M_PI);

    return angle_deg;
}
