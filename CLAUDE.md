# Autonomous Mining Truck Control System - Project Memory

## Project Overview

**Real-Time Automation (ATR) Course Project**
- Course: Real-Time Automation
- Semester: 2025.2
- Professor: Leandro Freitas
- Status: Stage 1 ✅ & Stage 2 ✅ Complete

### What This Project Does
Implements a distributed real-time control system for autonomous mining trucks with:
- **8 concurrent C++ tasks** running in parallel threads with circular buffer synchronization
- **Producer-Consumer pattern** for sensor data processing
- **State machine** for truck mode management (Manual/Automatic, OK/Fault)
- **Proportional controllers** for speed and steering navigation
- **Python GUIs** (Pygame simulation, Tkinter management) for Stage 2
- **MQTT pub/sub** communication for distributed system integration
- **CSV logging** for event persistence and analysis

---

## Real-Time Automation Concepts Covered

1. **Thread Synchronization**: Mutexes, condition variables, RAII pattern
2. **Producer-Consumer Pattern**: Bounded circular buffer (200 slots) with blocking operations
3. **State Machines**: Truck modes and fault handling with clear transitions
4. **Periodic Task Execution**: Fixed-interval scheduling (50-2000ms periods)
5. **Event-Driven Architecture**: Fault monitoring with callback notifications
6. **Control Systems**: P controllers for speed and angle with bumpless transfer
7. **Distributed Communication**: MQTT for multi-component system integration
8. **Data Persistence**: Structured CSV logging with timestamps

---

## Architecture

### 7 Core Tasks (All C++)
| Task | Period | Purpose |
|------|--------|---------|
| Sensor Processing | 100ms | Read sensors → Apply MA filter → Write to buffer (Producer) |
| Command Logic | 50ms | Read buffer → State machine → Generate outputs (Consumer) |
| Fault Monitoring | 100ms | Detect faults → Trigger callbacks (Event monitor) |
| Navigation Control | 50ms | P controllers for speed/steering → Bumpless transfer |
| Route Planning | On-demand | Set waypoint targets for navigation |
| Data Collector | 1000ms | Log events to CSV with timestamps |
| Local Interface | 2000ms | Display terminal UI (or AI-optimized logging) |

### Key Design Patterns

**Circular Buffer (Common Types)**
```cpp
CircularBuffer buffer;     // 200 slots, thread-safe
buffer.write(data);        // Producer: non-blocking with overwrite
SensorData d = buffer.read(); // Consumer: blocking if empty
SensorData peek = buffer.peek_latest(); // Non-blocking read
```

**Task Thread Pattern**
```cpp
class Task {
    std::atomic<bool> running_;
    std::thread task_thread_;
    
    void start() { running_ = true; task_thread_ = std::thread(&Task::task_loop, this); }
    void stop() { running_ = false; task_thread_.join(); }
    void task_loop() { 
        auto next = std::chrono::steady_clock::now();
        while(running_) {
            // Do work
            next += std::chrono::milliseconds(period_ms_);
            std::this_thread::sleep_until(next);
        }
    }
};
```

**State Machine (Command Logic)**
```
MANUAL/OK ←→ AUTO/OK (via auto_mode cmd, must not fault)
    ↓         ↓
  FAULT (detected by fault monitor)
    ↑
  (fault_rearmed cmd + temp < 120°C, no elec/hydr fault)
```

**Bumpless Transfer (Navigation Control)**
- When switching manual→auto: Update setpoints to CURRENT position/angle
- Controllers run with zero error, outputs smoothly reach steady state
- Prevents control jumps and mechanical stress

---

## Key Structures & Types

### SensorData (Circular Buffer Contents)
```cpp
struct SensorData {
    int position_x, position_y;      // Position (-∞ to +∞)
    int angle_x;                     // Heading degrees (0=East)
    int temperature;                 // -100°C to +200°C
    bool fault_electrical, fault_hydraulic;
    long timestamp;                  // Milliseconds since epoch
};
```

### Fault Detection Thresholds
- Temperature Alert: T > 95°C (Warning)
- Temperature Critical: T > 120°C (System Fault)
- Electrical Fault: fault_electrical = true
- Hydraulic Fault: fault_hydraulic = true

### Truck State Machine
```cpp
struct TruckState {
    bool fault;        // e_defeito: true = FAULT state
    bool automatic;    // e_automatico: true = AUTO, false = MANUAL
};
```

### Actuator Outputs (0-100% acceleration, ±180° steering)
```cpp
struct ActuatorOutput {
    int acceleration;  // -100 to 100 (%)
    int steering;      // -180 to 180 (degrees)
    bool arrived;      // Navigation reached target
};
```

---

## Control Parameters

### Speed Controller (P-only)
- Proportional gain Kp_speed = 0.15
- Max acceleration = 20
- Arrival distance threshold = 2.0 units
- Deceleration distance = 30.0 units (aggressive slowdown)

### Angle Controller (P-only)
- Proportional gain Kp_angle = 0.15
- Arrival angle threshold = 5.0 degrees
- Angle error normalized to [-180, 180]

### Moving Average Filter (Sensor Processing)
- Order M = 5 samples
- Formula: `y[n] = (x[n] + 4*y[n-1]) / 5` (exponential MA)
- Applied to: position_x, position_y, angle_x, temperature

---

## File Structure

```
.
├── src/               # C++ implementations
│   ├── main.cpp       # System startup, bridge communication, main loop
│   ├── circular_buffer.cpp
│   ├── sensor_processing.cpp
│   ├── command_logic.cpp
│   ├── fault_monitoring.cpp
│   ├── navigation_control.cpp
│   ├── route_planning.cpp
│   ├── data_collector.cpp
│   ├── local_interface.cpp
│   └── logger.cpp     # AI-optimized structured logging
├── include/           # C++ headers (mirror of src/)
├── python_gui/        # Stage 2 components
│   ├── mine_simulation.py       # Pygame physics engine
│   ├── mine_management.py       # Tkinter SCADA interface
│   └── mqtt_bridge.py           # File-based MQTT bridge (C++ ↔ MQTT)
├── build/             # CMake build output
├── logs/              # Runtime CSV logs
├── CMakeLists.txt
├── GUIDE.md           # Comprehensive documentation (100+ pages)
├── README.md
└── doc/               # Project specifications
```

---

## Build & Execution

### Build (C++ Core)
```bash
mkdir -p build && cd build
cmake ..
make
cd ..
./build/truck_control
```

### Stage 2 (Full System with GUIs)
```bash
# Option A: Automated script
./setup_and_run_stage2.sh

# Option B: Manual (5 terminals)
mosquitto                                    # Terminal 1: MQTT broker
python3 python_gui/mqtt_bridge.py           # Terminal 2: Bridge
python3 python_gui/mine_simulation.py       # Terminal 3: Simulation
python3 python_gui/mine_management.py       # Terminal 4: Management
./build/truck_control                       # Terminal 5: Truck control
```

### Expected Output
- Terminal displays truck status with ANSI colors (if VISUAL_UI=1)
- Structured logs: `timestamp|LEVEL|MODULE|key=val,key=val`
- CSV log file: `logs/truck_1_log.csv`
- Pygame window: Mine simulation with physics
- Tkinter window: Mine management map interface

---

## Logging System (AI-Optimized)

### Format
```
timestamp|level|module|event_data
1731283456789|INFO|SP|event=write,temp=75,pos_x=100,pos_y=200
```

### Module Codes
- MA=Main, SP=Sensor Processing, CB=Circular Buffer
- CL=Command Logic, FM=Fault Monitoring, NC=Navigation Control
- RP=Route Planning, DC=Data Collector, LI=Local Interface

### Log Levels (Set via LOG_LEVEL env var)
- DEBUG (0): Detailed diagnostics
- INFO (1): Lifecycle events (default)
- WARN (2): Warnings
- ERR (3): Errors
- CRIT (4): Critical failures

### Usage
```cpp
LOG_INFO(SP) << "temp" << 75 << "status" << "ok";
LOG_CRIT(FM) << "event" << "fault" << "type" << "TEMP_CRT";
```

---

## Common Tasks & Patterns

### Adding a New Task
1. Create `src/new_task.cpp` and `include/new_task.h`
2. Inherit thread + atomic running pattern
3. Implement `start()`, `stop()`, `task_loop()`
4. Use `peek_latest()` for non-blocking buffer reads
5. Use `read()` only if blocking is acceptable
6. Add logging: `LOG_INFO(MODULE) << "key" << value`

### Testing Fault Conditions (Stage 2)
- Press `T` in Mine Simulation: Increase temperature (+20°C per press)
- Press `E`: Toggle electrical fault
- Press `H`: Toggle hydraulic fault
- At T > 120°C: Truck enters FAULT state (red color)
- Command Logic disables actuators
- Use "Rearm Fault" in Mine Management to recover

### Reading CSV Logs
```bash
head -5 logs/truck_1_log.csv
# Fields: Timestamp,TruckID,State,PositionX,PositionY,Description

# Analyze with awk/grep:
grep "FAULT" logs/truck_1_log.csv  # Find fault events
grep "AUTO" logs/truck_1_log.csv   # Find mode switches
tail -20 logs/truck_1_log.csv      # Last 20 events
```

---

## Debugging Tips

### Issue: Hanging/Deadlock
- Check if buffer initialized BEFORE tasks start (main.cpp:58)
- Verify mutex unlock happens (use `std::lock_guard` for RAII)
- Check for circular wait in lock ordering
- Use `Ctrl+\` to force quit

### Issue: Truck Doesn't Move in Auto Mode
- Verify "Auto Mode" button clicked in Mine Management
- Check waypoint was sent (click "Send Waypoint")
- Ensure C++ truck_control is running
- Check MQTT messages: `mosquitto_sub -t "truck/1/#" -v`
- Verify no fault state (temp, electrical, hydraulic)

### Issue: Temperature Not Increasing
- In Mine Simulation: Press `T` key (10 times for +200°C total)
- Check sensor data reaches C++ (look in bridge/from_mqtt/)
- Verify Mosquitto broker running: `pgrep mosquitto`

### Checking MQTT Communication
```bash
# Terminal 1: Subscribe to all topics
mosquitto_sub -t "truck/#" -v

# Terminal 2: Publish test message
mosquitto_pub -t "truck/1/test" -m "hello"
```

---

## Performance Characteristics

### Task Periods (CPU Utilization Balance)
- Sensor Processing: 100ms (IO-bound, produces data)
- Command Logic: 50ms (Fast decision-making, consumes data)
- Fault Monitoring: 100ms (IO-bound, periodic checks)
- Navigation Control: 50ms (Real-time control loop)
- Data Collector: 1000ms (IO-bound, disk write)
- Local Interface: 2000ms (Low priority display)

### Circular Buffer
- Size: 200 slots fixed
- Behavior: Non-blocking write (overwrites oldest on full)
- Synchronization: Mutex + 2 condition variables (not_full, not_empty)
- Throughput: ~10 items/sec (100ms producer, 50ms consumer = 2x producer)

### MQTT Bridge (File-based)
- Messages written to: `bridge/from_mqtt/*.json`
- Messages read from: `bridge/to_mqtt/*.json`
- Latency: ~100-200ms (polling interval)
- Reliability: At most once (QoS 0)

---

## Important Notes

### Thread Safety
- All shared state protected by `std::mutex`
- Use `std::lock_guard` for automatic unlock (RAII)
- Use `std::unique_lock` only with condition variables
- No deadlocks if: hold at most 1 lock, lock ordering consistent

### Real-Time Constraints
- Hard deadline: 50ms (navigation control loop)
- Soft deadline: 100ms (fault monitoring)
- Log level set to INFO or WARN for production (DEBUG disabled)
- No dynamic memory allocation in task loops

### Stage 2 Specifics
- MQTT bridge uses file-based communication (no C++ MQTT lib)
- Mine Simulation: Pygame (physics engine, 30 FPS)
- Mine Management: Tkinter (map display, control interface)
- Communication: JSON over MQTT (Mosquitto broker)

---

## References

- **GUIDE.md**: 100+ page comprehensive guide (architecture, testing, code details)
- **README.md**: Quick start and feature overview
- **ProjectDescription.md**: Original requirements and specifications
- **LOGGING_SYSTEM.md**: Detailed logging design and examples
- **doc/RealTimeAutomation.md**: Course concepts and theory

---

## Quick Reminders

✅ **Before coding**: Read GUIDE.md section 7 (Implementation Details)
✅ **Add feature**: Follow task thread pattern (start/stop/task_loop)
✅ **Debug issue**: Check GUIDE.md section 8 (Troubleshooting)
✅ **Test scenario**: See GUIDE.md section 6 (Manual Testing)
✅ **Build fails**: Verify CMake 3.14+, C++17 support, pthread linked
✅ **MQTT issue**: Run `mosquitto` first, check bridge/from_mqtt/
✅ **Fault test**: Press T/E/H in Mine Simulation, watch truck change color
✅ **Log analysis**: CSV in logs/, structured logging on console

---

**Last Updated**: November 2025
**Project Status**: Stage 1 & 2 Complete ✅
**Next Steps**: Performance optimization, multi-truck scaling, advanced path planning
EOF
cat /home/claude/CLAUDE.md
Output
# Autonomous Mining Truck Control System - Project Memory

## Project Overview

**Real-Time Automation (ATR) Course Project**
- Course: Real-Time Automation
- Semester: 2025.2
- Professor: Leandro Freitas
- Status: Stage 1 ✅ & Stage 2 ✅ Complete

### What This Project Does
Implements a distributed real-time control system for autonomous mining trucks with:
- **8 concurrent C++ tasks** running in parallel threads with circular buffer synchronization
- **Producer-Consumer pattern** for sensor data processing
- **State machine** for truck mode management (Manual/Automatic, OK/Fault)
- **Proportional controllers** for speed and steering navigation
- **Python GUIs** (Pygame simulation, Tkinter management) for Stage 2
- **MQTT pub/sub** communication for distributed system integration
- **CSV logging** for event persistence and analysis

---

## Real-Time Automation Concepts Covered

1. **Thread Synchronization**: Mutexes, condition variables, RAII pattern
2. **Producer-Consumer Pattern**: Bounded circular buffer (200 slots) with blocking operations
3. **State Machines**: Truck modes and fault handling with clear transitions
4. **Periodic Task Execution**: Fixed-interval scheduling (50-2000ms periods)
5. **Event-Driven Architecture**: Fault monitoring with callback notifications
6. **Control Systems**: P controllers for speed and angle with bumpless transfer
7. **Distributed Communication**: MQTT for multi-component system integration
8. **Data Persistence**: Structured CSV logging with timestamps

---

## Architecture

### 7 Core Tasks (All C++)
| Task | Period | Purpose |
|------|--------|---------|
| Sensor Processing | 100ms | Read sensors → Apply MA filter → Write to buffer (Producer) |
| Command Logic | 50ms | Read buffer → State machine → Generate outputs (Consumer) |
| Fault Monitoring | 100ms | Detect faults → Trigger callbacks (Event monitor) |
| Navigation Control | 50ms | P controllers for speed/steering → Bumpless transfer |
| Route Planning | On-demand | Set waypoint targets for navigation |
| Data Collector | 1000ms | Log events to CSV with timestamps |
| Local Interface | 2000ms | Display terminal UI (or AI-optimized logging) |

### Key Design Patterns

**Circular Buffer (Common Types)**
```cpp
CircularBuffer buffer;     // 200 slots, thread-safe
buffer.write(data);        // Producer: non-blocking with overwrite
SensorData d = buffer.read(); // Consumer: blocking if empty
SensorData peek = buffer.peek_latest(); // Non-blocking read
```

**Task Thread Pattern**
```cpp
class Task {
    std::atomic<bool> running_;
    std::thread task_thread_;
    
    void start() { running_ = true; task_thread_ = std::thread(&Task::task_loop, this); }
    void stop() { running_ = false; task_thread_.join(); }
    void task_loop() { 
        auto next = std::chrono::steady_clock::now();
        while(running_) {
            // Do work
            next += std::chrono::milliseconds(period_ms_);
            std::this_thread::sleep_until(next);
        }
    }
};
```

**State Machine (Command Logic)**
```
MANUAL/OK ←→ AUTO/OK (via auto_mode cmd, must not fault)
    ↓         ↓
  FAULT (detected by fault monitor)
    ↑
  (fault_rearmed cmd + temp < 120°C, no elec/hydr fault)
```

**Bumpless Transfer (Navigation Control)**
- When switching manual→auto: Update setpoints to CURRENT position/angle
- Controllers run with zero error, outputs smoothly reach steady state
- Prevents control jumps and mechanical stress

---

## Key Structures & Types

### SensorData (Circular Buffer Contents)
```cpp
struct SensorData {
    int position_x, position_y;      // Position (-∞ to +∞)
    int angle_x;                     // Heading degrees (0=East)
    int temperature;                 // -100°C to +200°C
    bool fault_electrical, fault_hydraulic;
    long timestamp;                  // Milliseconds since epoch
};
```

### Fault Detection Thresholds
- Temperature Alert: T > 95°C (Warning)
- Temperature Critical: T > 120°C (System Fault)
- Electrical Fault: fault_electrical = true
- Hydraulic Fault: fault_hydraulic = true

### Truck State Machine
```cpp
struct TruckState {
    bool fault;        // e_defeito: true = FAULT state
    bool automatic;    // e_automatico: true = AUTO, false = MANUAL
};
```

### Actuator Outputs (0-100% acceleration, ±180° steering)
```cpp
struct ActuatorOutput {
    int acceleration;  // -100 to 100 (%)
    int steering;      // -180 to 180 (degrees)
    bool arrived;      // Navigation reached target
};
```

---

## Control Parameters

### Speed Controller (P-only)
- Proportional gain Kp_speed = 0.15
- Max acceleration = 20
- Arrival distance threshold = 2.0 units
- Deceleration distance = 30.0 units (aggressive slowdown)

### Angle Controller (P-only)
- Proportional gain Kp_angle = 0.15
- Arrival angle threshold = 5.0 degrees
- Angle error normalized to [-180, 180]

### Moving Average Filter (Sensor Processing)
- Order M = 5 samples
- Formula: `y[n] = (x[n] + 4*y[n-1]) / 5` (exponential MA)
- Applied to: position_x, position_y, angle_x, temperature

---

## File Structure

```
.
├── src/               # C++ implementations
│   ├── main.cpp       # System startup, bridge communication, main loop
│   ├── circular_buffer.cpp
│   ├── sensor_processing.cpp
│   ├── command_logic.cpp
│   ├── fault_monitoring.cpp
│   ├── navigation_control.cpp
│   ├── route_planning.cpp
│   ├── data_collector.cpp
│   ├── local_interface.cpp
│   └── logger.cpp     # AI-optimized structured logging
├── include/           # C++ headers (mirror of src/)
├── python_gui/        # Stage 2 components
│   ├── mine_simulation.py       # Pygame physics engine
│   ├── mine_management.py       # Tkinter SCADA interface
│   └── mqtt_bridge.py           # File-based MQTT bridge (C++ ↔ MQTT)
├── build/             # CMake build output
├── logs/              # Runtime CSV logs
├── CMakeLists.txt
├── GUIDE.md           # Comprehensive documentation (100+ pages)
├── README.md
└── doc/               # Project specifications
```

---

## Build & Execution

### Build (C++ Core)
```bash
mkdir -p build && cd build
cmake ..
make
cd ..
./build/truck_control
```

### Stage 2 (Full System with GUIs)
```bash
# Option A: Automated script
./setup_and_run_stage2.sh

# Option B: Manual (5 terminals)
mosquitto                                    # Terminal 1: MQTT broker
python3 python_gui/mqtt_bridge.py           # Terminal 2: Bridge
python3 python_gui/mine_simulation.py       # Terminal 3: Simulation
python3 python_gui/mine_management.py       # Terminal 4: Management
./build/truck_control                       # Terminal 5: Truck control
```

### Expected Output
- Terminal displays truck status with ANSI colors (if VISUAL_UI=1)
- Structured logs: `timestamp|LEVEL|MODULE|key=val,key=val`
- CSV log file: `logs/truck_1_log.csv`
- Pygame window: Mine simulation with physics
- Tkinter window: Mine management map interface

---

## Logging System (AI-Optimized)

### Format
```
timestamp|level|module|event_data
1731283456789|INFO|SP|event=write,temp=75,pos_x=100,pos_y=200
```

### Module Codes
- MA=Main, SP=Sensor Processing, CB=Circular Buffer
- CL=Command Logic, FM=Fault Monitoring, NC=Navigation Control
- RP=Route Planning, DC=Data Collector, LI=Local Interface

### Log Levels (Set via LOG_LEVEL env var)
- DEBUG (0): Detailed diagnostics
- INFO (1): Lifecycle events (default)
- WARN (2): Warnings
- ERR (3): Errors
- CRIT (4): Critical failures

### Usage
```cpp
LOG_INFO(SP) << "temp" << 75 << "status" << "ok";
LOG_CRIT(FM) << "event" << "fault" << "type" << "TEMP_CRT";
```

---

## Common Tasks & Patterns

### Adding a New Task
1. Create `src/new_task.cpp` and `include/new_task.h`
2. Inherit thread + atomic running pattern
3. Implement `start()`, `stop()`, `task_loop()`
4. Use `peek_latest()` for non-blocking buffer reads
5. Use `read()` only if blocking is acceptable
6. Add logging: `LOG_INFO(MODULE) << "key" << value`

### Testing Fault Conditions (Stage 2)
- Press `T` in Mine Simulation: Increase temperature (+20°C per press)
- Press `E`: Toggle electrical fault
- Press `H`: Toggle hydraulic fault
- At T > 120°C: Truck enters FAULT state (red color)
- Command Logic disables actuators
- Use "Rearm Fault" in Mine Management to recover

### Reading CSV Logs
```bash
head -5 logs/truck_1_log.csv
# Fields: Timestamp,TruckID,State,PositionX,PositionY,Description

# Analyze with awk/grep:
grep "FAULT" logs/truck_1_log.csv  # Find fault events
grep "AUTO" logs/truck_1_log.csv   # Find mode switches
tail -20 logs/truck_1_log.csv      # Last 20 events
```

---

## Debugging Tips

### Issue: Hanging/Deadlock
- Check if buffer initialized BEFORE tasks start (main.cpp:58)
- Verify mutex unlock happens (use `std::lock_guard` for RAII)
- Check for circular wait in lock ordering
- Use `Ctrl+\` to force quit

### Issue: Truck Doesn't Move in Auto Mode
- Verify "Auto Mode" button clicked in Mine Management
- Check waypoint was sent (click "Send Waypoint")
- Ensure C++ truck_control is running
- Check MQTT messages: `mosquitto_sub -t "truck/1/#" -v`
- Verify no fault state (temp, electrical, hydraulic)

### Issue: Temperature Not Increasing
- In Mine Simulation: Press `T` key (10 times for +200°C total)
- Check sensor data reaches C++ (look in bridge/from_mqtt/)
- Verify Mosquitto broker running: `pgrep mosquitto`

### Checking MQTT Communication
```bash
# Terminal 1: Subscribe to all topics
mosquitto_sub -t "truck/#" -v

# Terminal 2: Publish test message
mosquitto_pub -t "truck/1/test" -m "hello"
```

---

## Performance Characteristics

### Task Periods (CPU Utilization Balance)
- Sensor Processing: 100ms (IO-bound, produces data)
- Command Logic: 50ms (Fast decision-making, consumes data)
- Fault Monitoring: 100ms (IO-bound, periodic checks)
- Navigation Control: 50ms (Real-time control loop)
- Data Collector: 1000ms (IO-bound, disk write)
- Local Interface: 2000ms (Low priority display)

### Circular Buffer
- Size: 200 slots fixed
- Behavior: Non-blocking write (overwrites oldest on full)
- Synchronization: Mutex + 2 condition variables (not_full, not_empty)
- Throughput: ~10 items/sec (100ms producer, 50ms consumer = 2x producer)

### MQTT Bridge (File-based)
- Messages written to: `bridge/from_mqtt/*.json`
- Messages read from: `bridge/to_mqtt/*.json`
- Latency: ~100-200ms (polling interval)
- Reliability: At most once (QoS 0)

---

## Important Notes

### Thread Safety
- All shared state protected by `std::mutex`
- Use `std::lock_guard` for automatic unlock (RAII)
- Use `std::unique_lock` only with condition variables
- No deadlocks if: hold at most 1 lock, lock ordering consistent

### Real-Time Constraints
- Hard deadline: 50ms (navigation control loop)
- Soft deadline: 100ms (fault monitoring)
- Log level set to INFO or WARN for production (DEBUG disabled)
- No dynamic memory allocation in task loops

### Stage 2 Specifics
- MQTT bridge uses file-based communication (no C++ MQTT lib)
- Mine Simulation: Pygame (physics engine, 30 FPS)
- Mine Management: Tkinter (map display, control interface)
- Communication: JSON over MQTT (Mosquitto broker)

---

## References

- **GUIDE.md**: 100+ page comprehensive guide (architecture, testing, code details)
- **README.md**: Quick start and feature overview
- **ProjectDescription.md**: Original requirements and specifications
- **LOGGING_SYSTEM.md**: Detailed logging design and examples
- **doc/RealTimeAutomation.md**: Course concepts and theory

---

## Quick Reminders

✅ **Before coding**: Read GUIDE.md section 7 (Implementation Details)
✅ **Add feature**: Follow task thread pattern (start/stop/task_loop)
✅ **Debug issue**: Check GUIDE.md section 8 (Troubleshooting)
✅ **Test scenario**: See GUIDE.md section 6 (Manual Testing)
✅ **Build fails**: Verify CMake 3.14+, C++17 support, pthread linked
✅ **MQTT issue**: Run `mosquitto` first, check bridge/from_mqtt/
✅ **Fault test**: Press T/E/H in Mine Simulation, watch truck change color
✅ **Log analysis**: CSV in logs/, structured logging on console

---

**Last Updated**: November 2025
**Project Status**: Stage 1 & 2 Complete ✅
**Next Steps**: Performance optimization, multi-truck scaling, advanced path planning
