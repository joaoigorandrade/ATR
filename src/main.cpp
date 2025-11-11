#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <string>

// Include all task headers
#include "circular_buffer.h"
#include "sensor_processing.h"
#include "command_logic.h"
#include "fault_monitoring.h"
#include "navigation_control.h"
#include "route_planning.h"
#include "data_collector.h"
#include "local_interface.h"

// Simple JSON parsing for sensor data
#include <sstream>
#include <map>

namespace fs = std::filesystem;

// Global flag for graceful shutdown
std::atomic<bool> system_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n[Main] Shutdown signal received..." << std::endl;
        system_running = false;
    }
}

/**
 * @brief Simple JSON value extractor for sensor data
 * Extracts integer or boolean values from JSON strings
 */
int extract_json_int(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0;

    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0;

    size_t start = pos + 1;
    while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;

    size_t end = start;
    while (end < json.length() && (isdigit(json[end]) || json[end] == '-')) end++;

    if (end > start) {
        return std::stoi(json.substr(start, end - start));
    }
    return 0;
}

bool extract_json_bool(const std::string& json, const std::string& key) {
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return false;

    pos = json.find(":", pos);
    if (pos == std::string::npos) return false;

    size_t true_pos = json.find("true", pos);
    size_t false_pos = json.find("false", pos);

    if (true_pos != std::string::npos && (false_pos == std::string::npos || true_pos < false_pos)) {
        return true;
    }
    return false;
}

/**
 * @brief Read and process sensor data from MQTT bridge
 * Reads JSON files from bridge/from_mqtt/ directory containing sensor data
 */
bool read_sensor_data_from_bridge(RawSensorData& data) {
    const std::string bridge_dir = "bridge/from_mqtt";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        // Find all JSON files with "sensors" in the name
        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("sensors") != std::string::npos) {

                // Read file
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string json_content = buffer.str();
                file.close();

                // Extract the payload field first (MQTT bridge wraps data in payload)
                size_t payload_pos = json_content.find("\"payload\"");
                std::string payload_content = json_content;
                if (payload_pos != std::string::npos) {
                    // Find the opening brace of the payload object
                    size_t payload_start = json_content.find("{", payload_pos);
                    if (payload_start != std::string::npos) {
                        payload_content = json_content.substr(payload_start);
                    }
                }

                // Parse sensor data from payload
                data.position_x = extract_json_int(payload_content, "position_x");
                data.position_y = extract_json_int(payload_content, "position_y");
                data.angle_x = extract_json_int(payload_content, "angle_x");
                data.temperature = extract_json_int(payload_content, "temperature");
                data.fault_electrical = extract_json_bool(payload_content, "fault_electrical");
                data.fault_hydraulic = extract_json_bool(payload_content, "fault_hydraulic");

                // Delete processed file
                fs::remove(entry.path());

                return true;
            }
        }
    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }

    return false;
}

/**
 * @brief Read and process commands from MQTT bridge
 * Reads JSON files from bridge/from_mqtt/ directory containing command data
 */
bool read_commands_from_bridge(OperatorCommand& cmd) {
    const std::string bridge_dir = "bridge/from_mqtt";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        // Find all JSON files with "commands" in the name
        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("commands") != std::string::npos) {

                // Read file
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string json_content = buffer.str();
                file.close();

                // Extract the payload field first (MQTT bridge wraps data in payload)
                size_t payload_pos = json_content.find("\"payload\"");
                std::string payload_content = json_content;
                if (payload_pos != std::string::npos) {
                    // Find the opening brace of the payload object
                    size_t payload_start = json_content.find("{", payload_pos);
                    if (payload_start != std::string::npos) {
                        payload_content = json_content.substr(payload_start);
                    }
                }

                // Check if this is a mode command (not an actuator command)
                // Mode commands have auto_mode, manual_mode, or rearm fields
                // Actuator commands have acceleration and steering fields
                bool has_auto_mode = payload_content.find("\"auto_mode\"") != std::string::npos;
                bool has_manual_mode = payload_content.find("\"manual_mode\"") != std::string::npos;
                bool has_rearm = payload_content.find("\"rearm\"") != std::string::npos;

                // Skip actuator-only commands (our own published commands)
                if (!has_auto_mode && !has_manual_mode && !has_rearm) {
                    // This is an actuator command, not a mode command - delete and skip
                    fs::remove(entry.path());
                    continue;
                }

                // Parse command data from payload
                cmd.auto_mode = extract_json_bool(payload_content, "auto_mode");
                cmd.manual_mode = extract_json_bool(payload_content, "manual_mode");
                cmd.rearm = extract_json_bool(payload_content, "rearm");

                // Delete processed file
                fs::remove(entry.path());

                // Only log actual mode changes, not every command
                if (cmd.auto_mode || cmd.manual_mode || cmd.rearm) {
                    std::cout << "[Main] Mode command: auto=" << cmd.auto_mode
                              << " manual=" << cmd.manual_mode << " rearm=" << cmd.rearm << std::endl;
                }

                return true;
            }
        }
    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }

    return false;
}

/**
 * @brief Read and process setpoint data from MQTT bridge
 * Reads JSON files from bridge/from_mqtt/ directory containing setpoint data
 */
bool read_setpoint_from_bridge(NavigationSetpoint& setpoint) {
    const std::string bridge_dir = "bridge/from_mqtt";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        // Find all JSON files with "setpoint" in the name
        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("setpoint") != std::string::npos) {

                // Read file
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string json_content = buffer.str();
                file.close();

                // Extract the payload field first (MQTT bridge wraps data in payload)
                size_t payload_pos = json_content.find("\"payload\"");
                std::string payload_content = json_content;
                if (payload_pos != std::string::npos) {
                    // Find the opening brace of the payload object
                    size_t payload_start = json_content.find("{", payload_pos);
                    if (payload_start != std::string::npos) {
                        payload_content = json_content.substr(payload_start);
                    }
                }

                // Parse setpoint data from payload
                setpoint.target_position_x = extract_json_int(payload_content, "target_x");
                setpoint.target_position_y = extract_json_int(payload_content, "target_y");
                setpoint.target_speed = extract_json_int(payload_content, "target_speed");

                // Delete processed file
                fs::remove(entry.path());

                std::cout << "[Main] Received setpoint via MQTT: target=(" << setpoint.target_position_x
                          << "," << setpoint.target_position_y << "), speed=" << setpoint.target_speed << "%" << std::endl;

                return true;
            }
        }
    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }

    return false;
}

/**
 * @brief Write actuator commands to MQTT bridge
 * Writes JSON files to bridge/to_mqtt/ directory for MQTT publishing
 */
void write_actuator_commands_to_bridge(int truck_id, const ActuatorOutput& output) {
    const std::string bridge_dir = "bridge/to_mqtt";

    try {
        // Create directory if it doesn't exist
        if (!fs::exists(bridge_dir)) {
            fs::create_directories(bridge_dir);
        }

        // Generate unique filename with timestamp
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        long timestamp = ms.count();

        std::ostringstream filename;
        filename << bridge_dir << "/" << timestamp << "_truck_" << truck_id << "_commands.json";

        // Create JSON content
        std::ostringstream json_content;
        json_content << "{\n"
                    << "  \"topic\": \"truck/" << truck_id << "/commands\",\n"
                    << "  \"payload\": {\n"
                    << "    \"acceleration\": " << output.acceleration << ",\n"
                    << "    \"steering\": " << output.steering << ",\n"
                    << "    \"arrived\": " << (output.arrived ? "true" : "false") << "\n"
                    << "  }\n"
                    << "}";

        // Write to file
        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << json_content.str();
            file.close();
        }
    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }
}

/**
 * @brief Main entry point for Autonomous Truck Control System - Stage 1
 *
 * This integrates all core tasks:
 * - Sensor Processing (Producer)
 * - Command Logic (Consumer, State Machine)
 * - Fault Monitoring (Event Generator)
 * - Navigation Control (Controllers)
 * - Route Planning (Setpoint Generator)
 * - Data Collector (Logger)
 * - Local Interface (HMI)
 *
 * Stage 1 Demonstration:
 * - All blue tasks from Figure 1 are implemented
 * - Circular buffer with Producer-Consumer pattern
 * - Mutex and condition variable synchronization
 * - Periodic task execution
 * - Event-driven fault handling
 */
int main() {
    // Register signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);

    std::cout << "========================================" << std::endl;
    std::cout << "Autonomous Mining Truck Control System" << std::endl;
    std::cout << "Stage 1: Core Tasks Integration" << std::endl;
    std::cout << "Real-Time Automation Project" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Create shared circular buffer (200 positions as specified)
    CircularBuffer buffer;
    std::cout << "[Main] Circular buffer created (size: 200)" << std::endl;

    // Create all tasks
    std::cout << "\n[Main] Creating tasks..." << std::endl;

    SensorProcessing sensor_task(buffer, 5, 100);        // Filter order 5, 100ms period
    CommandLogic command_task(buffer, 50);               // 50ms period
    FaultMonitoring fault_task(buffer, 100);             // 100ms period
    NavigationControl nav_task(buffer, 50);              // 50ms control period
    RoutePlanning route_planner;                         // No periodic task
    DataCollector data_collector(buffer, 1, 1000);       // Truck ID 1, 1s logging
    LocalInterface local_interface(buffer, 2000);        // 2s display update

    std::cout << "[Main] All tasks created successfully" << std::endl;

    // Set up initial configuration
    std::cout << "\n[Main] Configuring system..." << std::endl;

    // Set a target waypoint for demonstration
    route_planner.set_target_waypoint(500, 300, 50);  // Target (500,300), speed 50%

    // Simulate initial sensor data
    RawSensorData initial_data;
    initial_data.position_x = 100;
    initial_data.position_y = 200;
    initial_data.angle_x = 0;
    initial_data.temperature = 75;  // Normal temperature
    initial_data.fault_electrical = false;
    initial_data.fault_hydraulic = false;
    sensor_task.set_raw_data(initial_data);

    // Start all tasks
    std::cout << "\n[Main] Starting all tasks..." << std::endl;
    std::cout << "========================================\n" << std::endl;

    sensor_task.start();
    command_task.start();
    fault_task.start();
    nav_task.start();
    data_collector.start();

    // Small delay to let tasks initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Start local interface last (it will clear the screen)
    local_interface.start();

    std::cout << "[Main] System running. Press Ctrl+C to stop." << std::endl;

    // Main loop: Stage 2 - Read sensor data from MQTT bridge
    RawSensorData current_data = initial_data;
    int bridge_read_count = 0;
    while (system_running) {
        // Try to read sensor data from MQTT bridge
        RawSensorData bridge_data;
        if (read_sensor_data_from_bridge(bridge_data)) {
            current_data = bridge_data;
            sensor_task.set_raw_data(current_data);

            // Reduced logging frequency: every 50 sensor readings (roughly 5 seconds)
            if (++bridge_read_count % 50 == 0) {
                std::cout << "[Main] Sensor update: temp=" << bridge_data.temperature
                          << "°C, pos=(" << bridge_data.position_x << "," << bridge_data.position_y << ")" << std::endl;
            }
        }

        // Try to read commands from MQTT bridge
        OperatorCommand bridge_cmd;
        if (read_commands_from_bridge(bridge_cmd)) {
            command_task.set_command(bridge_cmd);
        }

        // Try to read setpoint from MQTT bridge
        NavigationSetpoint bridge_setpoint;
        if (read_setpoint_from_bridge(bridge_setpoint)) {
            route_planner.set_target_waypoint(bridge_setpoint.target_position_x,
                                              bridge_setpoint.target_position_y,
                                              bridge_setpoint.target_speed);
        }

        // Update states between tasks
        TruckState state = command_task.get_state();
        nav_task.set_truck_state(state);
        data_collector.set_truck_state(state);
        local_interface.set_truck_state(state);

        // Get current position from buffer to calculate target angle
        SensorData current_sensor = buffer.peek_latest();

        // Get setpoint and calculate target angle based on current position
        NavigationSetpoint setpoint = route_planner.get_setpoint();
        setpoint.target_angle = route_planner.calculate_target_angle(
            current_sensor.position_x, current_sensor.position_y);
        nav_task.set_setpoint(setpoint);

        ActuatorOutput nav_output = nav_task.get_output();
        command_task.set_navigation_output(nav_output);

        ActuatorOutput actuator_output = command_task.get_actuator_output();
        local_interface.set_actuator_output(actuator_output);

        // Publish actuator commands to MQTT (for simulation)
        write_actuator_commands_to_bridge(1, actuator_output);

        // Short sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Graceful shutdown
    std::cout << "\n\n[Main] Shutting down tasks..." << std::endl;

    local_interface.stop();
    data_collector.stop();
    nav_task.stop();
    fault_task.stop();
    command_task.stop();
    sensor_task.stop();

    std::cout << "\n========================================" << std::endl;
    std::cout << "System shutdown complete." << std::endl;
    std::cout << "Stage 1 demonstration finished." << std::endl;
    std::cout << "Check logs/ directory for event logs." << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
