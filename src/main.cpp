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
#include "watchdog.h"
#include "performance_monitor.h"
#include <sstream>
#include <map>
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

std::atomic<bool> system_running(true);
PerformanceMonitor* global_perf_monitor = nullptr;

void signal_handler(int signal) {
    if (signal == SIGINT) {
        LOG_INFO(MAIN) << "event" << "shutdown_signal";

        // Print performance report on shutdown
        if (global_perf_monitor) {
            std::cout << "\n";
            global_perf_monitor->print_report();
        }

        system_running = false;
    }
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
                }

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

                json j = json::parse(file);
                file.close();

                if (j.contains("payload")) {
                    auto& payload = j["payload"];

                    bool has_auto_mode = payload.contains("auto_mode");
                    bool has_manual_mode = payload.contains("manual_mode");
                    bool has_rearm = payload.contains("rearm");

                    if (!has_auto_mode && !has_manual_mode && !has_rearm) {
                        fs::remove(entry.path());
                        continue;
                    }

                    cmd.auto_mode = payload.value("auto_mode", false);
                    cmd.manual_mode = payload.value("manual_mode", false);
                    cmd.rearm = payload.value("rearm", false);

                    if (cmd.auto_mode || cmd.manual_mode || cmd.rearm) {
                        LOG_INFO(MAIN) << "event" << "cmd_recv"
                                       << "auto" << cmd.auto_mode
                                       << "manual" << cmd.manual_mode
                                       << "rearm" << cmd.rearm;
                    }
                }

                fs::remove(entry.path());
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

                json j = json::parse(file);
                file.close();

                if (j.contains("payload")) {
                    auto& payload = j["payload"];
                    setpoint.target_position_x = payload.value("target_x", 0);
                    setpoint.target_position_y = payload.value("target_y", 0);
                    setpoint.target_speed = payload.value("target_speed", 0);
                }

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
        // Silently ignore errors - bridge might not be ready yet
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

    // Create performance monitor
    PerformanceMonitor perf_monitor;
    global_perf_monitor = &perf_monitor;

    // Register tasks with expected periods
    perf_monitor.register_task("SensorProcessing", 20);
    perf_monitor.register_task("CommandLogic", 10);
    perf_monitor.register_task("FaultMonitoring", 20);
    perf_monitor.register_task("NavigationControl", 10);
    perf_monitor.register_task("DataCollector", 1000);
    perf_monitor.register_task("LocalInterface", 2000);

    LOG_INFO(MAIN) << "event" << "perf_monitor_init" << "tasks" << 6;

    CircularBuffer buffer;
    LOG_INFO(MAIN) << "event" << "buffer_create" << "size" << 200;


    LOG_DEBUG(MAIN) << "event" << "creating_tasks";

    SensorProcessing sensor_task(buffer, 5, 20, &perf_monitor);
    CommandLogic command_task(buffer, 10, &perf_monitor);
    FaultMonitoring fault_task(buffer, 20, &perf_monitor);
    NavigationControl nav_task(buffer, 10, &perf_monitor);
    RoutePlanning route_planner;
    DataCollector data_collector(buffer, 1, 1000, &perf_monitor);
    LocalInterface local_interface(buffer, 2000, &perf_monitor);

    LOG_DEBUG(MAIN) << "event" << "tasks_created";

    // Create watchdog for fault tolerance monitoring
    Watchdog watchdog(100);  // Check every 100ms
    Watchdog::set_instance(&watchdog);  // Set global instance for task access
    watchdog.register_task("SensorProcessing", 60);    // 3x period (20ms * 3)
    watchdog.register_task("CommandLogic", 30);        // 3x period (10ms * 3)
    watchdog.register_task("FaultMonitoring", 60);     // 3x period (20ms * 3)
    watchdog.register_task("NavigationControl", 30);   // 3x period (10ms * 3)
    watchdog.register_task("DataCollector", 3000);     // 3x period (1000ms * 3)

    LOG_DEBUG(MAIN) << "event" << "watchdog_configured" << "tasks" << watchdog.get_task_count();


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
    watchdog.start();  // Start watchdog monitoring


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

        // Publish actuator commands and truck state via MQTT bridge
        write_actuator_commands_to_bridge(1, actuator_output);
        write_truck_state_to_bridge(1, state);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
