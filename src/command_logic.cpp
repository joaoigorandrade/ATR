#include "command_logic.h"
#include "logger.h"
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

    LOG_INFO(CL) << "event" << "init" << "period_ms" << period_ms_;
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

    LOG_INFO(CL) << "event" << "start";
}

void CommandLogic::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    LOG_INFO(CL) << "event" << "stop";
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
        // Peek at latest sensor data (non-consuming read)
        // Multiple tasks need to read sensor data, so we use peek instead of read
        SensorData sensor_data = buffer_.peek_latest();

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
                    LOG_CRIT(CL) << "event" << "fault_detect";
                }
                current_state_.fault = true;
                fault_rearmed_ = false;
            } else if (current_state_.fault && fault_rearmed_) {
                // Fault cleared and operator rearmed
                LOG_INFO(CL) << "event" << "fault_clear";
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
            LOG_INFO(CL) << "event" << "mode_change" << "mode" << "auto";
        } else {
            LOG_WARN(CL) << "event" << "mode_reject" << "reason" << "fault";
        }
    }

    if (pending_command_.manual_mode && current_state_.automatic) {
        current_state_.automatic = false;
        LOG_INFO(CL) << "event" << "mode_change" << "mode" << "manual";
    }

    // Process fault rearm command
    if (pending_command_.rearm && current_state_.fault) {
        fault_rearmed_ = true;
        LOG_INFO(CL) << "event" << "rearm_ack";
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
