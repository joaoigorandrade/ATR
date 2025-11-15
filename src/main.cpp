#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <string>
#include "logger.h"
#include "circular_buffer.h"
#include "sensor_processing.h"
#include "command_logic.h"
#include "fault_monitoring.h"
#include "navigation_control.h"
#include "route_planning.h"
#include "data_collector.h"
#include "local_interface.h"
#include <sstream>
#include <map>

namespace fs = std::filesystem;

std::atomic<bool> system_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        LOG_INFO(MAIN) << "event" << "shutdown_signal";
        system_running = false;
    }
}


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


bool read_sensor_data_from_bridge(RawSensorData& data) {
    const std::string bridge_dir = "bridge/from_mqtt";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("sensors") != std::string::npos) {

                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string json_content = buffer.str();
                file.close();

                size_t payload_pos = json_content.find("\"payload\"");
                std::string payload_content = json_content;
                if (payload_pos != std::string::npos) {
                    size_t payload_start = json_content.find("{", payload_pos);
                    if (payload_start != std::string::npos) {
                        payload_content = json_content.substr(payload_start);
                    }
                }

                data.position_x = extract_json_int(payload_content, "position_x");
                data.position_y = extract_json_int(payload_content, "position_y");
                data.angle_x = extract_json_int(payload_content, "angle_x");
                data.temperature = extract_json_int(payload_content, "temperature");
                data.fault_electrical = extract_json_bool(payload_content, "fault_electrical");
                data.fault_hydraulic = extract_json_bool(payload_content, "fault_hydraulic");

                fs::remove(entry.path());

                return true;
            }
        }
    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }

    return false;
}


bool read_commands_from_bridge(OperatorCommand& cmd) {
    const std::string bridge_dir = "bridge/from_mqtt";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("commands") != std::string::npos) {

                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string json_content = buffer.str();
                file.close();

                size_t payload_pos = json_content.find("\"payload\"");
                std::string payload_content = json_content;
                if (payload_pos != std::string::npos) {
                    size_t payload_start = json_content.find("{", payload_pos);
                    if (payload_start != std::string::npos) {
                        payload_content = json_content.substr(payload_start);
                    }
                }

                bool has_auto_mode = payload_content.find("\"auto_mode\"") != std::string::npos;
                bool has_manual_mode = payload_content.find("\"manual_mode\"") != std::string::npos;
                bool has_rearm = payload_content.find("\"rearm\"") != std::string::npos;

                if (!has_auto_mode && !has_manual_mode && !has_rearm) {
                    fs::remove(entry.path());
                    continue;
                }

                cmd.auto_mode = extract_json_bool(payload_content, "auto_mode");
                cmd.manual_mode = extract_json_bool(payload_content, "manual_mode");
                cmd.rearm = extract_json_bool(payload_content, "rearm");

                fs::remove(entry.path());

                if (cmd.auto_mode || cmd.manual_mode || cmd.rearm) {
                    LOG_INFO(MAIN) << "event" << "cmd_recv"
                                   << "auto" << cmd.auto_mode
                                   << "manual" << cmd.manual_mode
                                   << "rearm" << cmd.rearm;
                }

                return true;
            }
        }
    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }

    return false;
}


bool read_setpoint_from_bridge(NavigationSetpoint& setpoint) {
    const std::string bridge_dir = "bridge/from_mqtt";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("setpoint") != std::string::npos) {

                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string json_content = buffer.str();
                file.close();

                size_t payload_pos = json_content.find("\"payload\"");
                std::string payload_content = json_content;
                if (payload_pos != std::string::npos) {
                    size_t payload_start = json_content.find("{", payload_pos);
                    if (payload_start != std::string::npos) {
                        payload_content = json_content.substr(payload_start);
                    }
                }

                setpoint.target_position_x = extract_json_int(payload_content, "target_x");
                setpoint.target_position_y = extract_json_int(payload_content, "target_y");
                setpoint.target_speed = extract_json_int(payload_content, "target_speed");

                fs::remove(entry.path());

                LOG_INFO(MAIN) << "event" << "setpoint_recv"
                               << "tgt_x" << setpoint.target_position_x
                               << "tgt_y" << setpoint.target_position_y
                               << "speed" << setpoint.target_speed;

                return true;
            }
        }
    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }

    return false;
}


void write_actuator_commands_to_bridge(int truck_id, const ActuatorOutput& output) {
    const std::string bridge_dir = "bridge/to_mqtt";

    try {

        if (!fs::exists(bridge_dir)) {
            fs::create_directories(bridge_dir);
        }


        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        long timestamp = ms.count();

        std::ostringstream filename;
        filename << bridge_dir << "/" << timestamp << "_truck_" << truck_id << "_commands.json";


        std::ostringstream json_content;
        json_content << "{\n"
                    << "  \"topic\": \"truck/" << truck_id << "/commands\",\n"
                    << "  \"payload\": {\n"
                    << "    \"acceleration\": " << output.acceleration << ",\n"
                    << "    \"steering\": " << output.steering << ",\n"
                    << "    \"arrived\": " << (output.arrived ? "true" : "false") << "\n"
                    << "  }\n"
                    << "}";



        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << json_content.str();
            file.close();
        }

    } catch (const std::exception& e) {
        // Silently ignore errors - bridge might not be ready yet
    }
}

int main() {

    Logger::init(Logger::Level::INFO);


    std::signal(SIGINT, signal_handler);

    std::cout << "========================================" << std::endl;
    std::cout << "Autonomous Mining Truck Control System" << std::endl;
    std::cout << "Stage 2: Full Integration" << std::endl;
    std::cout << "Real-Time Automation Project" << std::endl;
    std::cout << "========================================" << std::endl;

    LOG_INFO(MAIN) << "event" << "system_start" << "stage" << 2;


    CircularBuffer buffer;
    LOG_INFO(MAIN) << "event" << "buffer_create" << "size" << 200;


    LOG_DEBUG(MAIN) << "event" << "creating_tasks";

    SensorProcessing sensor_task(buffer, 5, 100);
    CommandLogic command_task(buffer, 50);
    FaultMonitoring fault_task(buffer, 100);
    NavigationControl nav_task(buffer, 50);
    RoutePlanning route_planner;
    DataCollector data_collector(buffer, 1, 1000);
    LocalInterface local_interface(buffer, 2000);

    LOG_DEBUG(MAIN) << "event" << "tasks_created";


    LOG_DEBUG(MAIN) << "event" << "configuring";

    route_planner.set_target_waypoint(500, 300, 50);


    RawSensorData initial_data;
    initial_data.position_x = 100;
    initial_data.position_y = 200;
    initial_data.angle_x = 0;
    initial_data.temperature = 75;
    initial_data.fault_electrical = false;
    initial_data.fault_hydraulic = false;
    sensor_task.set_raw_data(initial_data);


    LOG_DEBUG(MAIN) << "event" << "starting_tasks";

    sensor_task.start();
    command_task.start();
    fault_task.start();
    nav_task.start();
    data_collector.start();


    std::this_thread::sleep_for(std::chrono::milliseconds(500));


    local_interface.start();

    LOG_INFO(MAIN) << "event" << "system_ready";


    RawSensorData current_data = initial_data;
    int bridge_read_count = 0;
    while (system_running) {

        RawSensorData bridge_data;
        if (read_sensor_data_from_bridge(bridge_data)) {
            current_data = bridge_data;
            sensor_task.set_raw_data(current_data);


            if (++bridge_read_count % 100 == 0) {
                LOG_DEBUG(MAIN) << "event" << "sensor_update"
                                << "temp" << bridge_data.temperature
                                << "pos_x" << bridge_data.position_x
                                << "pos_y" << bridge_data.position_y;
            }
        }


        OperatorCommand bridge_cmd;
        if (read_commands_from_bridge(bridge_cmd)) {
            command_task.set_command(bridge_cmd);
        }


        NavigationSetpoint bridge_setpoint;
        if (read_setpoint_from_bridge(bridge_setpoint)) {
            route_planner.set_target_waypoint(bridge_setpoint.target_position_x,
                                              bridge_setpoint.target_position_y,
                                              bridge_setpoint.target_speed);
        }


        TruckState state = command_task.get_state();
        nav_task.set_truck_state(state);
        data_collector.set_truck_state(state);
        local_interface.set_truck_state(state);


        SensorData current_sensor = buffer.peek_latest();


        NavigationSetpoint setpoint = route_planner.get_setpoint();
        setpoint.target_angle = route_planner.calculate_target_angle(
            current_sensor.position_x, current_sensor.position_y);
        nav_task.set_setpoint(setpoint);

        ActuatorOutput nav_output = nav_task.get_output();
        command_task.set_navigation_output(nav_output);

        ActuatorOutput actuator_output = command_task.get_actuator_output();
        local_interface.set_actuator_output(actuator_output);


        write_actuator_commands_to_bridge(1, actuator_output);


        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }


    LOG_INFO(MAIN) << "event" << "shutdown_start";

    local_interface.stop();
    data_collector.stop();
    nav_task.stop();
    fault_task.stop();
    command_task.stop();
    sensor_task.stop();

    std::cout << "\n========================================" << std::endl;
    std::cout << "System shutdown complete." << std::endl;
    std::cout << "Check logs/ directory for event logs." << std::endl;
    std::cout << "========================================" << std::endl;

    LOG_INFO(MAIN) << "event" << "shutdown_complete";

    return 0;
}
