#include "command_logic.h"
#include <iostream>
#include <chrono>

CommandLogic::CommandLogic(CircularBuffer& buffer, int period_ms)
    : buffer_(buffer),
      period_ms_(period_ms),
      running_(false),
      command_pending_(false),
      fault_rearmed_(false) {

    // Initialize state to manual mode, no fault
    current_state_.fault = false;
    current_state_.automatic = false;

    // Initialize sensor data with safe defaults
    latest_sensor_data_ = {};

    std::cout << "[Command Logic] Initialized (period: " << period_ms_ << "ms)" << std::endl;
}

CommandLogic::~CommandLogic() {
    stop();
}

void CommandLogic::start() {
    if (running_) {
        return;
    }

    running_ = true;
    task_thread_ = std::thread(&CommandLogic::task_loop, this);

    std::cout << "[Command Logic] Task started" << std::endl;
}

void CommandLogic::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    std::cout << "[Command Logic] Task stopped" << std::endl;
}

void CommandLogic::set_command(const OperatorCommand& cmd) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    pending_command_ = cmd;
    command_pending_ = true;
}

TruckState CommandLogic::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
}

ActuatorOutput CommandLogic::get_actuator_output() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return actuator_output_;
}

SensorData CommandLogic::get_latest_sensor_data() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return latest_sensor_data_;
}

void CommandLogic::set_navigation_output(const ActuatorOutput& output) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    navigation_output_ = output;
}

void CommandLogic::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {
        // Read sensor data from circular buffer (Consumer operation)
        SensorData sensor_data = buffer_.read();

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            latest_sensor_data_ = sensor_data;

            // Check for faults in sensor data
            bool fault_detected = check_faults(sensor_data);

            // Process any pending operator commands
            if (command_pending_) {
                process_commands();
                command_pending_ = false;
            }

            // Update fault state
            if (fault_detected) {
                if (!current_state_.fault) {
                    std::cout << "[Command Logic] FAULT DETECTED!" << std::endl;
                }
                current_state_.fault = true;
                fault_rearmed_ = false;
            } else if (current_state_.fault && fault_rearmed_) {
                // Fault cleared and operator rearmed
                std::cout << "[Command Logic] Fault cleared and rearmed" << std::endl;
                current_state_.fault = false;
                fault_rearmed_ = false;
            }

            // Calculate actuator outputs based on current mode
            calculate_actuator_outputs();
        }

        // Wait until next execution time (periodic task)
        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

void CommandLogic::process_commands() {
    // Process mode switching commands
    if (pending_command_.auto_mode && !current_state_.automatic) {
        if (!current_state_.fault) {
            current_state_.automatic = true;
            std::cout << "[Command Logic] Switched to AUTOMATIC mode" << std::endl;
        } else {
            std::cout << "[Command Logic] Cannot switch to automatic: FAULT present" << std::endl;
        }
    }

    if (pending_command_.manual_mode && current_state_.automatic) {
        current_state_.automatic = false;
        std::cout << "[Command Logic] Switched to MANUAL mode" << std::endl;
    }

    // Process fault rearm command
    if (pending_command_.rearm && current_state_.fault) {
        fault_rearmed_ = true;
        std::cout << "[Command Logic] Fault REARM acknowledged" << std::endl;
    }
}

bool CommandLogic::check_faults(const SensorData& data) {
    // Check temperature thresholds
    // Alert at T > 95°C, Critical fault at T > 120°C
    if (data.temperature > 120) {
        return true;  // Critical temperature fault
    }

    // Check electrical and hydraulic faults
    if (data.fault_electrical || data.fault_hydraulic) {
        return true;
    }

    return false;
}

void CommandLogic::calculate_actuator_outputs() {
    if (current_state_.fault) {
        // FAULT STATE: Set actuators to safe values (stop)
        actuator_output_.acceleration = 0;
        actuator_output_.steering = 0;
        return;
    }

    if (current_state_.automatic) {
        // AUTOMATIC MODE: Use navigation control outputs
        actuator_output_ = navigation_output_;
    } else {
        // MANUAL MODE: Use operator commands
        actuator_output_.acceleration = pending_command_.accelerate;

        // Steering: combine left/right commands
        // Left increases angle, right decreases angle
        int steering_delta = pending_command_.steer_left - pending_command_.steer_right;
        actuator_output_.steering += steering_delta;

        // Clamp steering to valid range [-180, 180]
        if (actuator_output_.steering > 180) {
            actuator_output_.steering = 180;
        } else if (actuator_output_.steering < -180) {
            actuator_output_.steering = -180;
        }

        // Clamp acceleration to valid range [-100, 100]
        if (actuator_output_.acceleration > 100) {
            actuator_output_.acceleration = 100;
        } else if (actuator_output_.acceleration < -100) {
            actuator_output_.acceleration = -100;
        }
    }
}
