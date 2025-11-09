#include <iostream>
#include <thread>
#include <chrono>
#include "circular_buffer.h"
#include "sensor_processing.h"

/**
 * @brief Main entry point for Autonomous Truck Control System
 *
 * This is a placeholder main that demonstrates the circular buffer functionality.
 * In later stages, this will initialize and coordinate all real-time tasks.
 */

void test_sensor_processing() {
    std::cout << "Testing Sensor Processing..." << std::endl;

    CircularBuffer buffer;
    SensorProcessing sensor_task(buffer, 5, 50);  // Filter order 5, 50ms period

    // Start sensor processing task
    sensor_task.start();

    // Simulate raw sensor data with noise
    for (int i = 0; i < 10; i++) {
        RawSensorData raw_data;
        raw_data.position_x = 100 + (i % 3) - 1;  // Add small noise
        raw_data.position_y = 200 + (i % 3) - 1;
        raw_data.angle_x = 45;
        raw_data.temperature = 85 + (i % 5) - 2;  // Temperature varies
        raw_data.fault_electrical = false;
        raw_data.fault_hydraulic = false;

        sensor_task.set_raw_data(raw_data);
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }

    // Read some processed data
    std::cout << "\nReading processed sensor data:" << std::endl;
    for (int i = 0; i < 3; i++) {
        SensorData data = buffer.read();
        std::cout << "  Data " << (i+1) << ": pos=(" << data.position_x << ", "
                  << data.position_y << "), angle=" << data.angle_x
                  << ", temp=" << data.temperature << "°C" << std::endl;
    }

    sensor_task.stop();
    std::cout << "Sensor processing test passed!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Autonomous Mining Truck Control System" << std::endl;
    std::cout << "Real-Time Automation Project" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Test sensor processing with circular buffer
    test_sensor_processing();

    std::cout << std::endl;
    std::cout << "System initialized successfully." << std::endl;
    std::cout << "Full system integration coming in next stages..." << std::endl;

    return 0;
}
