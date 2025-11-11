#include "local_interface.h"
#include "logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <unistd.h>  // for isatty()
#include <cstdlib>   // for getenv()

LocalInterface::LocalInterface(CircularBuffer& buffer, int update_period_ms)
    : buffer_(buffer),
      update_period_ms_(update_period_ms),
      running_(false) {

    // Initialize with safe defaults
    truck_state_.fault = false;
    truck_state_.automatic = false;
    latest_sensor_data_ = {};
    buffer_count_ = 0;

    LOG_INFO(LI) << "event" << "init" << "period_ms" << update_period_ms_;
}

LocalInterface::~LocalInterface() {
    stop();
}

void LocalInterface::start() {
    if (running_) {
        return;
    }

    running_ = true;
    task_thread_ = std::thread(&LocalInterface::task_loop, this);

    LOG_INFO(LI) << "event" << "start";
}

void LocalInterface::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    LOG_INFO(LI) << "event" << "stop";
}

void LocalInterface::set_truck_state(const TruckState& state) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    truck_state_ = state;
}

void LocalInterface::set_actuator_output(const ActuatorOutput& output) {
    std::lock_guard<std::mutex> lock(display_mutex_);
    actuator_output_ = output;
}

void LocalInterface::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {
        // Peek at latest sensor data
        size_t buffer_count = buffer_.size();
        SensorData sensor_data = buffer_.peek_latest();

        {
            std::lock_guard<std::mutex> lock(display_mutex_);
            latest_sensor_data_ = sensor_data;
            // Store buffer count for debugging
            buffer_count_ = buffer_count;
        }

        // Display status
        display_status();

        // Wait until next execution time
        next_execution += std::chrono::milliseconds(update_period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

void LocalInterface::display_status() {
    std::lock_guard<std::mutex> lock(display_mutex_);

    // AI-FRIENDLY: Structured status snapshot (always logged)
    LOG_INFO(LI) << "status" << "snapshot"
                 << "mode" << (truck_state_.automatic ? "AUTO" : "MAN")
                 << "fault" << (truck_state_.fault ? 1 : 0)
                 << "x" << latest_sensor_data_.position_x
                 << "y" << latest_sensor_data_.position_y
                 << "ang" << latest_sensor_data_.angle_x
                 << "temp" << latest_sensor_data_.temperature
                 << "elec" << (latest_sensor_data_.fault_electrical ? 1 : 0)
                 << "hydr" << (latest_sensor_data_.fault_hydraulic ? 1 : 0)
                 << "acc" << actuator_output_.acceleration
                 << "str" << actuator_output_.steering
                 << "arr" << (actuator_output_.arrived ? 1 : 0);

    // HUMAN-FRIENDLY: Visual display control
    // Default: AI mode (logs only), set VISUAL_UI=1 to enable visual display
    static bool visual_enabled = []() -> bool {
        const char* env = std::getenv("VISUAL_UI");
        if (env) {
            std::string val(env);
            return (val == "1" || val == "true" || val == "TRUE");
        }
        // Default: AI mode (no visual UI)
        return false;
    }();

    if (visual_enabled) {
        // Clear screen
        std::cout << "\033[2J\033[1;1H";

        // Compact header with status
        std::cout << "=== TRUCK [";
        if (truck_state_.fault) {
            std::cout << "\033[1;31mFAULT\033[0m";  // Red
        } else if (truck_state_.automatic) {
            std::cout << "\033[1;32mAUTO\033[0m";   // Green
        } else {
            std::cout << "\033[1;33mMANUAL\033[0m"; // Yellow
        }
        std::cout << "] ===" << std::endl;

        // Single-line sensor status
        std::cout << "POS:(" << latest_sensor_data_.position_x << ","
                  << latest_sensor_data_.position_y << ") "
                  << "HDG:" << latest_sensor_data_.angle_x << "° ";

        // Temperature with color coding
        if (latest_sensor_data_.temperature > 120) {
            std::cout << "\033[1;31mTEMP:" << latest_sensor_data_.temperature << "°C[CRIT]\033[0m ";
        } else if (latest_sensor_data_.temperature > 95) {
            std::cout << "\033[1;33mTEMP:" << latest_sensor_data_.temperature << "°C[WARN]\033[0m ";
        } else {
            std::cout << "TEMP:" << latest_sensor_data_.temperature << "°C ";
        }

        // Fault indicators
        if (latest_sensor_data_.fault_electrical) {
            std::cout << "\033[1;31m[ELEC]\033[0m ";
        }
        if (latest_sensor_data_.fault_hydraulic) {
            std::cout << "\033[1;31m[HYDR]\033[0m ";
        }
        std::cout << std::endl;

        // Actuator outputs
        std::cout << "ACC:" << std::setw(4) << actuator_output_.acceleration << "% "
                  << "STR:" << std::setw(4) << actuator_output_.steering << "° ";
        if (actuator_output_.arrived) {
            std::cout << "\033[1;32m[ARRIVED]\033[0m";
        }
        std::cout << std::endl;

        // Commands help
        std::cout << "CMD: A=Auto M=Manual R=Rearm W/S=Accel A/D=Steer" << std::endl;
        std::cout << "==========================================" << std::endl;
    }
}
