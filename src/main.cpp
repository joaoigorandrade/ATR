#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

// Include all task headers
#include "circular_buffer.h"
#include "sensor_processing.h"
#include "command_logic.h"
#include "fault_monitoring.h"
#include "navigation_control.h"
#include "route_planning.h"
#include "data_collector.h"
#include "local_interface.h"

// Global flag for graceful shutdown
std::atomic<bool> system_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n[Main] Shutdown signal received..." << std::endl;
        system_running = false;
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
    initial_data.angle_x = 45;
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

    // Main loop: simulate changing conditions
    int iteration = 0;
    while (system_running) {
        std::this_thread::sleep_for(std::chrono::seconds(3));

        iteration++;

        // Every 10 iterations, simulate a scenario
        if (iteration % 10 == 0) {
            // Scenario 1: Temperature increase (demonstration)
            RawSensorData hot_data = initial_data;
            hot_data.temperature = 100;  // High but not critical
            sensor_task.set_raw_data(hot_data);

            // Log event
            data_collector.log_event("TEST", initial_data.position_x,
                                    initial_data.position_y,
                                    "Temperature increased for testing");
        }

        // Update states between tasks (simplified for Stage 1)
        // In full integration, these would be automatic via shared memory/IPC
        TruckState state = command_task.get_state();
        nav_task.set_truck_state(state);
        data_collector.set_truck_state(state);
        local_interface.set_truck_state(state);

        NavigationSetpoint setpoint = route_planner.get_setpoint();
        nav_task.set_setpoint(setpoint);

        ActuatorOutput nav_output = nav_task.get_output();
        command_task.set_navigation_output(nav_output);

        ActuatorOutput actuator_output = command_task.get_actuator_output();
        local_interface.set_actuator_output(actuator_output);

        // Simulate truck movement (very simplified)
        // In Stage 2, this will be done by Mine Simulation
        if (!state.fault && state.automatic) {
            // Move truck slightly towards target
            initial_data.position_x += (actuator_output.acceleration > 0) ? 5 : 0;
            initial_data.position_y += (actuator_output.acceleration > 0) ? 3 : 0;
            sensor_task.set_raw_data(initial_data);
        }
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
