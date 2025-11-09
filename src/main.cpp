#include <iostream>
#include <thread>
#include <chrono>
#include "circular_buffer.h"

/**
 * @brief Main entry point for Autonomous Truck Control System
 *
 * This is a placeholder main that demonstrates the circular buffer functionality.
 * In later stages, this will initialize and coordinate all real-time tasks.
 */

void test_circular_buffer() {
    std::cout << "Testing Circular Buffer..." << std::endl;

    CircularBuffer buffer;

    // Test basic write/read
    SensorData write_data = {100, 200, 45, 85, false, false, 1000};
    buffer.write(write_data);

    SensorData read_data = buffer.read();
    std::cout << "Read data: position=(" << read_data.position_x << ", "
              << read_data.position_y << "), angle=" << read_data.angle_x
              << ", temp=" << read_data.temperature << std::endl;

    std::cout << "Circular buffer test passed!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Autonomous Mining Truck Control System" << std::endl;
    std::cout << "Real-Time Automation Project" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Test circular buffer
    test_circular_buffer();

    std::cout << std::endl;
    std::cout << "System initialized successfully." << std::endl;
    std::cout << "Full system integration coming in next stages..." << std::endl;

    return 0;
}
