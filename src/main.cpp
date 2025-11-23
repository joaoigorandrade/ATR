#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "logger.h"
#include "circular_buffer.h"
#include "sensor_processing.h"
#include "command_logic.h"
#include "fault_monitoring.h"
#include "navigation_control.h"
#include "route_planning.h"
#include "data_collector.h"
#include "local_interface.h"
#include "watchdog.h"
#include "performance_monitor.h"
#include <sstream>
#include <map>
#include "json.hpp"

constexpr int SENSOR_PROCESSING_PERIOD_MS = 20;
constexpr int COMMAND_LOGIC_PERIOD_MS = 10;
constexpr int FAULT_MONITORING_PERIOD_MS = 20;
constexpr int NAVIGATION_CONTROL_PERIOD_MS = 10;
constexpr int DATA_COLLECTOR_PERIOD_MS = 100;
constexpr int LOCAL_INTERFACE_PERIOD_MS = 100;
constexpr int NUMBER_OF_REGISTERED_TASKS_PERF = 6;

constexpr int CIRCULAR_BUFFER_SIZE = 200;
constexpr int WATCHDOG_CHECK_PERIOD_MS = 100;

constexpr int SENSOR_PROCESSING_WATCHDOG_TIMEOUT_MS = 60;
constexpr int COMMAND_LOGIC_WATCHDOG_TIMEOUT_MS = 30;
constexpr int FAULT_MONITORING_WATCHDOG_TIMEOUT_MS = 60;
constexpr int NAVIGATION_CONTROL_WATCHDOG_TIMEOUT_MS = 30;
constexpr int DATA_COLLECTOR_WATCHDOG_TIMEOUT_MS = 300;

constexpr int SENSOR_FILTER_ORDER = 5;
int g_truck_id = 1;

using json = nlohmann::json;
namespace fs = std::filesystem;

std::atomic<bool> system_running(true);
PerformanceMonitor* global_perf_monitor = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        LOG_INFO(MAIN) << "event" << "shutdown_signal";

        if (global_perf_monitor) {
            std::cout << "\n";
            global_perf_monitor->print_report();
        }

        system_running = false;
    }
}


bool read_sensor_data_from_bridge(RawSensorData& data) {
    const std::string bridge_dir = "bridge/from_mqtt";
    std::vector<fs::path> sensor_files;
    std::string search_pattern = "truck_" + std::to_string(g_truck_id) + "_sensors";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        // Collect all sensor files
        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find(search_pattern) != std::string::npos) {
                sensor_files.push_back(entry.path());
            }
        }

        if (sensor_files.empty()) {
            return false;
        }

    
        std::sort(sensor_files.begin(), sensor_files.end());

        
        bool success = false;
        const auto& newest_file = sensor_files.back();

        std::ifstream file(newest_file);
        if (file.is_open()) {
            json j = json::parse(file);
            file.close();

            if (j.contains("payload")) {
                auto& payload = j["payload"];
                data.position_x = payload.value("position_x", 0);
                data.position_y = payload.value("position_y", 0);
                data.angle_x = payload.value("angle_x", 0);
                data.temperature = payload.value("temperature", 0);
                data.fault_electrical = payload.value("fault_electrical", false);
                data.fault_hydraulic = payload.value("fault_hydraulic", false);
                success = true;
            }
        }

        for (const auto& path : sensor_files) {
            fs::remove(path);
        }

        return success;

    } catch (const std::exception& e) {
        // Log error if needed
    }

    return false;
}


bool read_commands_from_bridge(OperatorCommand& cmd) {
    const std::string bridge_dir = "bridge/from_mqtt";
    std::vector<fs::path> command_files;
    std::string search_pattern = "truck_" + std::to_string(g_truck_id) + "_commands";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find(search_pattern) != std::string::npos) {
                command_files.push_back(entry.path());
            }
        }

        if (command_files.empty()) {
            return false;
        }

    
        std::sort(command_files.begin(), command_files.end());

    
        bool success = false;
        const auto& newest_file = command_files.back();

        std::ifstream file(newest_file);
        if (file.is_open()) {
            json j = json::parse(file);
            file.close();

            if (j.contains("payload")) {
                auto& payload = j["payload"];

                bool has_auto_mode = payload.contains("auto_mode");
                bool has_manual_mode = payload.contains("manual_mode");
                bool has_rearm = payload.contains("rearm");
                bool has_accelerate = payload.contains("accelerate");
                bool has_steer_left = payload.contains("steer_left");
                bool has_steer_right = payload.contains("steer_right");

                if (has_auto_mode || has_manual_mode || has_rearm ||
                    has_accelerate || has_steer_left || has_steer_right) {
                    cmd.auto_mode = payload.value("auto_mode", false);
                    cmd.manual_mode = payload.value("manual_mode", false);
                    cmd.rearm = payload.value("rearm", false);
                    cmd.accelerate = payload.value("accelerate", 0);
                    cmd.steer_left = payload.value("steer_left", 0);
                    cmd.steer_right = payload.value("steer_right", 0);

                    if (cmd.auto_mode || cmd.manual_mode || cmd.rearm) {
                        LOG_INFO(MAIN) << "event" << "cmd_recv"
                                       << "auto" << cmd.auto_mode
                                       << "manual" << cmd.manual_mode
                                       << "rearm" << cmd.rearm;
                    }
                    
                    if (has_accelerate || has_steer_left || has_steer_right) {
                         LOG_DEBUG(MAIN) << "event" << "cmd_manual" 
                                         << "acc" << cmd.accelerate
                                         << "left" << cmd.steer_left
                                         << "right" << cmd.steer_right;
                    }
                    success = true;
                }
            }
        }

        for (const auto& path : command_files) {
            fs::remove(path);
        }

        return success;

    } catch (const std::exception& e) {
        // Log error
    }

    return false;
}


bool read_setpoint_from_bridge(NavigationSetpoint& setpoint) {
    const std::string bridge_dir = "bridge/from_mqtt";
    std::vector<fs::path> setpoint_files;
    std::string search_pattern = "truck_" + std::to_string(g_truck_id) + "_setpoint";

    try {
        if (!fs::exists(bridge_dir)) {
            return false;
        }

        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find(search_pattern) != std::string::npos) {
                setpoint_files.push_back(entry.path());
            }
        }

        if (setpoint_files.empty()) {
            return false;
        }

        
        std::sort(setpoint_files.begin(), setpoint_files.end());

        
        bool success = false;
        const auto& newest_file = setpoint_files.back();

        std::ifstream file(newest_file);
        if (file.is_open()) {
            json j = json::parse(file);
            file.close();

            if (j.contains("payload")) {
                auto& payload = j["payload"];
                setpoint.target_position_x = payload.value("target_x", 0);
                setpoint.target_position_y = payload.value("target_y", 0);
                setpoint.target_speed = payload.value("target_speed", 0);
                
                LOG_INFO(MAIN) << "event" << "setpoint_recv"
                               << "tgt_x" << setpoint.target_position_x
                               << "tgt_y" << setpoint.target_position_y
                               << "speed" << setpoint.target_speed;
                success = true;
            }
        }

        for (const auto& path : setpoint_files) {
            fs::remove(path);
        }

        return success;

    } catch (const std::exception& e) {
        // Log error
    }

    return false;
}

bool read_obstacles_from_bridge(std::vector<Obstacle>& obstacles) {
    const std::string bridge_dir = "bridge/from_mqtt";
    std::vector<fs::path> obstacle_files;
    std::string search_pattern = "truck_" + std::to_string(g_truck_id) + "_obstacles";

    try {
        if (!fs::exists(bridge_dir)) return false;

        for (const auto& entry : fs::directory_iterator(bridge_dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find(search_pattern) != std::string::npos) {
                obstacle_files.push_back(entry.path());
            }
        }

        if (obstacle_files.empty()) return false;

        std::sort(obstacle_files.begin(), obstacle_files.end());
        const auto& newest_file = obstacle_files.back();
        bool success = false;

        std::ifstream file(newest_file);
        if (file.is_open()) {
            json j = json::parse(file);
            file.close();
            if (j.contains("payload") && j["payload"].contains("obstacles")) {
                obstacles.clear();
                for (const auto& item : j["payload"]["obstacles"]) {
                    Obstacle obs;
                    obs.id = item.value("id", 0);
                    obs.x = item.value("x", 0);
                    obs.y = item.value("y", 0);
                    obstacles.push_back(obs);
                }
                success = true;
            }
        }
        
        for (const auto& path : obstacle_files) {
            fs::remove(path);
        }
        return success;
    } catch (...) { return false; }
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

        json j = {
            {"topic", "truck/" + std::to_string(truck_id) + "/commands"},
            {"payload", {
                {"acceleration", output.velocity},
                {"steering", output.steering},
                {"arrived", output.arrived}
            }}
        };

        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << j.dump(2);
            file.close();
        }

    } catch (const std::exception& e) {
    }
}

void write_truck_state_to_bridge(int truck_id, const TruckState& state) {
    const std::string bridge_dir = "bridge/to_mqtt";

    try {
        if (!fs::exists(bridge_dir)) {
            fs::create_directories(bridge_dir);
        }

        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        long timestamp = ms.count();

        std::ostringstream filename;
        filename << bridge_dir << "/" << timestamp << "_truck_" << truck_id << "_state.json";

        json j = {
            {"topic", "truck/" + std::to_string(truck_id) + "/state"},
            {"payload", {
                {"automatic", state.automatic},
                {"fault", state.fault}
            }}
        };

        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << j.dump(2);
            file.close();
        }

    } catch (const std::exception& e) {
    }
}

int main(int argc, char* argv[]) {

    Logger::init(Logger::Level::INFO);

    if (argc > 1) {
        try {
            g_truck_id = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid truck ID provided, using default: " << g_truck_id << std::endl;
        }
    }

    std::signal(SIGINT, signal_handler);

    std::cout << "========================================" << std::endl;
    std::cout << "Autonomous Mining Truck Control System" << std::endl;
    std::cout << "Stage 2: Full Integration" << std::endl;
    std::cout << "Truck ID: " << g_truck_id << std::endl;
    std::cout << "========================================" << std::endl;

    LOG_INFO(MAIN) << "event" << "system_start" << "stage" << 2 << "truck_id" << g_truck_id;

    PerformanceMonitor perf_monitor;
    global_perf_monitor = &perf_monitor;

    perf_monitor.register_task("SensorProcessing", SENSOR_PROCESSING_PERIOD_MS);
    perf_monitor.register_task("CommandLogic", COMMAND_LOGIC_PERIOD_MS);
    perf_monitor.register_task("FaultMonitoring", FAULT_MONITORING_PERIOD_MS);
    perf_monitor.register_task("NavigationControl",NAVIGATION_CONTROL_PERIOD_MS);
    perf_monitor.register_task("DataCollector", DATA_COLLECTOR_PERIOD_MS);
    perf_monitor.register_task("LocalInterface", LOCAL_INTERFACE_PERIOD_MS);

    LOG_INFO(MAIN) << "event" << "perf_monitor_init" << "tasks" << NUMBER_OF_REGISTERED_TASKS_PERF;

    CircularBuffer buffer;
    LOG_INFO(MAIN) << "event" << "buffer_create" << "size" << CIRCULAR_BUFFER_SIZE;


    LOG_DEBUG(MAIN) << "event" << "creating_tasks";

    SensorProcessing sensor_task(buffer, SENSOR_FILTER_ORDER, SENSOR_PROCESSING_PERIOD_MS, &perf_monitor);
    CommandLogic command_task(buffer, COMMAND_LOGIC_PERIOD_MS, &perf_monitor);
    FaultMonitoring fault_task(buffer, FAULT_MONITORING_PERIOD_MS, &perf_monitor);
    NavigationControl nav_task(buffer, NAVIGATION_CONTROL_PERIOD_MS, &perf_monitor);
    RoutePlanning route_planner;
    DataCollector data_collector(buffer, g_truck_id, DATA_COLLECTOR_PERIOD_MS, &perf_monitor);
    LocalInterface local_interface(buffer, LOCAL_INTERFACE_PERIOD_MS, &perf_monitor);

    LOG_DEBUG(MAIN) << "event" << "tasks_created";

    fault_task.register_fault_callback(
        [&](FaultType type, const SensorData& data) {
            command_task.on_fault_update(type);
            nav_task.on_fault_update(type);
            
            std::string desc = "Fault detected: " + std::to_string(static_cast<int>(type));
            if (type == FaultType::NONE) {
                desc = "Fault cleared";
            }
            data_collector.log_event(type == FaultType::NONE ? "OK" : "FAULT", 
                                   data.position_x, data.position_y, desc);
        }
    );

    Watchdog watchdog(WATCHDOG_CHECK_PERIOD_MS);
    Watchdog::set_instance(&watchdog);
    watchdog.register_task("SensorProcessing", SENSOR_PROCESSING_WATCHDOG_TIMEOUT_MS);
    watchdog.register_task("CommandLogic", COMMAND_LOGIC_WATCHDOG_TIMEOUT_MS);
    watchdog.register_task("FaultMonitoring", FAULT_MONITORING_WATCHDOG_TIMEOUT_MS);
    watchdog.register_task("NavigationControl",NAVIGATION_CONTROL_WATCHDOG_TIMEOUT_MS);
    watchdog.register_task("DataCollector", DATA_COLLECTOR_WATCHDOG_TIMEOUT_MS);

    LOG_DEBUG(MAIN) << "event" << "watchdog_configured" << "tasks" << watchdog.get_task_count();


    LOG_DEBUG(MAIN) << "event" << "configuring";

    route_planner.set_target_waypoint(500, 300, 50);


    RawSensorData initial_data;
    initial_data.position_x = 100 + (g_truck_id * 50); // Offset initial position
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
    watchdog.start();


    std::this_thread::sleep_for(std::chrono::milliseconds(500));


    local_interface.start();

    LOG_INFO(MAIN) << "event" << "system_ready";


    RawSensorData current_data = initial_data;
    int bridge_read_count = 0;

    ActuatorOutput last_actuator_output{};
    last_actuator_output.velocity = -999;
    last_actuator_output.steering = -999;
    last_actuator_output.arrived = false;

    TruckState last_state{};
    last_state.automatic = false;
    last_state.fault = false;

    int loop_counter = 0;
    constexpr int STATE_UPDATE_INTERVAL = 4;

    while (system_running) {
        loop_counter++;

        RawSensorData bridge_data;
        if (read_sensor_data_from_bridge(bridge_data)) {
            current_data = bridge_data;
            sensor_task.set_raw_data(current_data);


            if (++bridge_read_count % 250 == 0) {
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

        // Read obstacles
        std::vector<Obstacle> obstacles;
        if (read_obstacles_from_bridge(obstacles)) {
            route_planner.update_obstacles(obstacles);
        }

        TruckState state = command_task.get_state();
        nav_task.set_truck_state(state);
        data_collector.set_truck_state(state);
        local_interface.set_truck_state(state);


        SensorData current_sensor = buffer.peek_latest();

        // Use adjusted setpoint for obstacle avoidance
        NavigationSetpoint setpoint = route_planner.calculate_adjusted_setpoint(
            current_sensor.position_x, current_sensor.position_y);
        
        // Calculate target angle based on adjusted setpoint
        int dx = setpoint.target_position_x - current_sensor.position_x;
        int dy = setpoint.target_position_y - current_sensor.position_y;
        double angle_rad = std::atan2(dy, dx);
        setpoint.target_angle = static_cast<int>(angle_rad * 180.0 / M_PI);

        nav_task.set_setpoint(setpoint);

        ActuatorOutput nav_output = nav_task.get_output();
        command_task.set_navigation_output(nav_output);

        ActuatorOutput actuator_output = command_task.get_actuator_output();
        local_interface.set_actuator_output(actuator_output);

        bool force_update = (loop_counter % STATE_UPDATE_INTERVAL == 0);

        if (actuator_output.velocity != last_actuator_output.velocity ||
            actuator_output.steering != last_actuator_output.steering ||
            actuator_output.arrived != last_actuator_output.arrived ||
            force_update) {
            write_actuator_commands_to_bridge(g_truck_id, actuator_output);
            last_actuator_output = actuator_output;
        }

        if (state.automatic != last_state.automatic ||
            state.fault != last_state.fault ||
            force_update) {
            write_truck_state_to_bridge(g_truck_id, state);
            last_state = state;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }


    LOG_INFO(MAIN) << "event" << "shutdown_start";

    watchdog.stop();  // Stop watchdog first
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