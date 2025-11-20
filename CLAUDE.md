# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Core Coding Principles

All code generated or modified in this repository must adhere to the following strict quality standards.

### 1\. Strict "No Comments" Policy

  * **Never write comments in the source code.**
  * Do not use `//` or `/* */`.
  * Code must be **self-documenting**.
  * If logic is complex enough to require a comment, refactor it into a clearly named function or variable.
  * *Exception:* CMakeLists.txt or configuration files where syntax requires specific delimiters for functional reasons, but avoid explanatory text.

### 2\. Comprehensibility

  * Use verbose, descriptive naming conventions (e.g., `calculate_next_execution_time()` instead of `calc_next()`).
  * Avoid magic numbers; define them as `constexpr` or `const` variables with descriptive names.
  * Control flow must be linear and obvious.

### 3\. Testability

  * Design components to be isolated.
  * Inject dependencies via constructors rather than accessing global state or singletons directly within logic methods.
  * Ensure pure functions are separated from side-effect-heavy I/O operations.

### 4\. Low Coupling

  * Adhere strictly to the defined interfaces (Headers).
  * Components should interact only through specific integration points (e.g., `CircularBuffer`, `FaultRegistry`).
  * Avoid circular dependencies between include files.

### 5\. High Cohesion

  * **Single Responsibility Principle**: A class or module must handle exactly one aspect of the system (e.g., `SpeedController` should not handle `FileIO`).
  * Group related functions into specific namespaces or classes.

### 6\. Easy Maintenance

  * Follow standard C++17 idioms (RAII, smart pointers, `std::optional`).
  * Format code consistently (standard indentation, bracket placement).
  * Dead code must be deleted, not commented out.

-----

## Project Overview

This is an **Autonomous Mining Truck Control System** - a real-time embedded system implementing concurrent programming concepts. The system integrates onboard truck control (C++17), MQTT-based communication (Python bridge), mine simulation GUI (Pygame), and central mine management GUI (Tkinter).

## Build and Development Commands

### Building the C++ Application

```bash
# Clean build
rm -rf build && mkdir build && cd build && cmake .. && make && cd ..

# Quick rebuild (from root)
cd build && make && cd ..

# Build with debug symbols
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make && cd ..

# Build optimized release
mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make && cd ..
```

### Running the System

```bash
# Run complete system (all components)
./setup_and_run_stage2.sh

# Run C++ truck control only
./build/truck_control

# Run with specific log level
LOG_LEVEL=DEBUG ./build/truck_control
LOG_LEVEL=WARN ./build/truck_control

# Run individual Python components
python3 python_gui/mqtt_bridge.py
python3 python_gui/mine_simulation.py
python3 python_gui/mine_management.py
```

### Testing and Debugging

```bash
# Build with ThreadSanitizer (deadlock detection)
g++ -fsanitize=thread -g -O1 src/*.cpp -o truck_control -Iinclude -pthread -std=c++17

# Check for running processes
pgrep -a mosquitto
pgrep -a python3

# Monitor MQTT messages
mosquitto_sub -t "truck/+/#" -v

# View structured logs
grep "|CRT|" logs/*.log
grep "|FM|" logs/*.log
grep "event=mode_change" logs/*.log
```

## High-Level Architecture

### System Components and Data Flow

```
┌─────────────────┐    JSON Files    ┌──────────────────┐
│   Mine Sim GUI  │ ──────MQTT─────> │   MQTT Bridge    │
│    (Pygame)     │                  │    (Python)      │
└─────────────────┘                  └──────────────────┘
                                            │ │
                             ┌──────────────┘ └──────────────┐
                             │ bridge/from_mqtt (JSON files) │
                             └───────────────────────────────┘
                                             │
                                             ▼
                             ┌───────────────────────────────┐
                             │    C++ Truck Control          │
                             │  (Concurrent Thread Tasks)    │
                             │                               │
                             │  - Sensor Processing          │
                             │  - Command Logic              │
                             │  - Fault Monitoring           │
                             │  - Navigation Control         │
                             │  - Route Planning             │
                             │  - Data Collector             │
                             │  - Local Interface            │
                             │                               │
                             │  Managed by: Watchdog         │
                             │  Monitored by: PerfMonitor    │
                             └───────────────────────────────┘
                                             │
                             ┌──────────────┘
                             │ bridge/to_mqtt (JSON files)
                             └───────────────────────────────┐
                                             │ │             │
                             ┌──────────────┘ └──────────────┤
                             ▼                               ▼
                   ┌──────────────────┐            ┌──────────────────┐
                   │   MQTT Bridge    │            │  Mine Mgmt GUI   │
                   │    (Python)      │ ────MQTT───│   (Tkinter)      │
                   └──────────────────┘            └──────────────────┘
```

### Key Architectural Patterns

**1. Producer-Consumer with Circular Buffer**

  * `CircularBuffer` class: Thread-safe fixed-size buffer (200 positions default).
  * **Producer**: `SensorProcessing` task writes filtered sensor data.
  * **Consumers**: `CommandLogic`, `NavigationControl`, `DataCollector` read data.
  * Uses `std::mutex` and `std::condition_variable` for synchronization.
  * Implements `peek_latest()` for non-blocking reads.

**2. Lock Ordering Hierarchy (Deadlock Prevention)**

```cpp
Level 1: CircularBuffer::mutex_
Level 2: SensorProcessing::raw_data_mutex_
Level 3: FaultMonitoring::fault_mutex_
Level 4: CommandLogic::state_mutex_
Level 5: NavigationControl::control_mutex_
Level 6: FaultMonitoring::callback_mutex_
```

  * **Critical**: Always acquire locks in this order.
  * Use `std::scoped_lock` for multiple locks.

**3. Observer Pattern for Fault Notifications**

  * `FaultMonitoring` maintains callback registry.
  * Notifies `CommandLogic`, `NavigationControl`, and `DataCollector` on faults.
  * Fault types: `TEMPERATURE_ALERT`, `TEMPERATURE_CRITICAL`, `ELECTRICAL`, `HYDRAULIC`.

**4. File-Based IPC with MQTT Bridge**

  * C++ writes JSON to `bridge/to_mqtt/` (truck state, position, events).
  * C++ reads JSON from `bridge/from_mqtt/` (commands, setpoints, sensor data).
  * Python bridge translates between file I/O and MQTT pub/sub.
  * Bridge errors are silently ignored.

**5. Watchdog Pattern for Task Health Monitoring**

  * Tasks call `report_heartbeat()` periodically.
  * Watchdog detects hangs and logs critical alerts.

**6. RAII-Based Performance Monitoring**

  * `PerformanceMonitor` tracks execution time, WCET, jitter, deadline violations.
  * Zero overhead when disabled.
  * Generates formatted report on SIGINT.

## Critical Implementation Details

### Concurrency Management

**Thread Lifecycle Pattern (Reference Implementation):**

```cpp
class Task {
    std::thread thread_;
    std::atomic<bool> running_{false};

    void start() {
        running_ = true;
        thread_ = std::thread(&Task::task_loop, this);
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    void task_loop() {
        auto next_execution = std::chrono::steady_clock::now();
        while (running_) {
            auto start_time = std::chrono::steady_clock::now();

            perform_task_logic();

            if (perf_monitor_) {
                perf_monitor_->end_measurement("TaskName", start_time);
            }

            next_execution += std::chrono::milliseconds(period_ms_);
            std::this_thread::sleep_until(next_execution);
        }
    }
};
```

**Task Periods:**

  * `SensorProcessing`: 100ms (10 Hz)
  * `CommandLogic`: 50ms (20 Hz)
  * `FaultMonitoring`: 100ms (10 Hz)
  * `NavigationControl`: 50ms (20 Hz)
  * `DataCollector`: 1000ms (1 Hz)
  * `LocalInterface`: 2000ms (0.5 Hz)

### Logging System (AI-Optimized)

**Format:** `timestamp|level|module|key1=val1,key2=val2`

**Modules:** `MA` (Main), `SP` (Sensor), `CB` (Buffer), `CL` (Command), `FM` (Fault), `NC` (Nav), `RP` (Route), `DC` (Data), `LI` (Interface).

**Levels:** `DBG`, `INF`, `WRN`, `ERR`, `CRT`.

**Example:**

```cpp
LOG_INFO(SP) << "event" << "write" << "temp" << 75 << "pos_x" << 100;
LOG_CRIT(FM) << "event" << "fault" << "type" << "TEMP_CRT" << "temp" << 125;
```

### Navigation Control

**Two Independent PID Controllers:**

1.  **Speed Controller**: Maintains target velocity.
2.  **Angle Controller**: Maintains target heading.

**Bumpless Transfer:**

  * When switching manual to automatic, setpoint is initialized to current value.

**Arrival Detection:**

  * Within 5 units of target position.
  * Within ±5° of target angle.

### State Machine (Command Logic)

```
States: { MANUAL, AUTOMATIC }
Fault conditions override all states.

MANUAL → AUTOMATIC: cmd.auto_mode && !fault
AUTOMATIC → MANUAL: cmd.manual_mode
Fault cleared: cmd.rearm (requires fault condition resolution)
```

## Implementation Conventions

### Adding New Tasks

1.  Inherit from base pattern.
2.  Register with `Watchdog` in main.
3.  Register with `PerformanceMonitor`.
4.  Adhere to lock ordering hierarchy.
5.  Use structured `LOG_*` macros.
6.  Implement clean stop/start lifecycle.

### Synchronization Rules

  * **Never** acquire locks in reverse order.
  * **Always** use `std::lock_guard` or `std::scoped_lock`.
  * **Never** manually unlock.

### Working with the Bridge

  * **Read path**: `bridge/from_mqtt/*.json`
  * **Write path**: `bridge/to_mqtt/*.json`
  * Use `nlohmann/json` library.
  * Delete JSON files immediately after reading.

## Dependencies

  * CMake 3.14+
  * C++17 compiler (GCC 7+, Clang 5+)
  * pthread library
  * Python 3 (`paho-mqtt`, `pygame`, `tkinter`)
  * Mosquitto MQTT broker
  * `nlohmann/json` (Included header-only)

## Common Pitfalls

1.  Forgetting to delete JSON files after reading.
2.  Acquiring locks in wrong order.
3.  Not checking `running_` flag frequently.
4.  Logging too verbosely.
5.  Modifying buffer size without updating consumers.
6.  Not registering task with watchdog.
7.  Hardcoding task periods.
