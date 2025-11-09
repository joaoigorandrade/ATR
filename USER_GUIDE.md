# Autonomous Mining Truck Control System - User Guide

## Table of Contents
1. [Overview](#overview)
2. [Installation](#installation)
3. [Running the System](#running-the-system)
4. [System Features](#system-features)
5. [Code Flow and Architecture](#code-flow-and-architecture)
6. [Real-Time Automation Concepts](#real-time-automation-concepts)
7. [Troubleshooting](#troubleshooting)

---

## Overview

This project implements a simplified autonomous truck control system for mining operations, demonstrating key real-time automation concepts from the ATR course. The system is divided into two stages:

**Stage 1 (Implemented):** Core C++ tasks with circular buffer synchronization
**Stage 2 (Pending):** Python GUIs and MQTT communication

---

## Installation

### Prerequisites
- **C++ Compiler:** g++ or clang++ with C++17 support
- **CMake:** Version 3.14 or higher
- **pthread:** Usually included in Unix-based systems (Linux, macOS)

### Build Instructions

```bash
# Clone/navigate to project directory
cd /path/to/Project

# Create build directory and compile
mkdir -p build
cd build
cmake ..
make

# The executable will be created as: build/truck_control
```

---

## Running the System

### Basic Execution

```bash
# From project root directory
./build/truck_control
```

### Expected Output

The system will:
1. Initialize all 7 core tasks
2. Start the circular buffer
3. Display real-time truck status on terminal (updates every 2 seconds)
4. Log events to `logs/truck_1_log.csv`

### Stopping the System

Press `Ctrl+C` to gracefully shut down all tasks.

```
[Main] Shutdown signal received...
[Main] Shutting down tasks...
...
System shutdown complete.
```

---

## System Features

### 1. Sensor Processing
**Command:** Automatically started
**Result:** Filters sensor data using moving average (order 5)

**Code Flow:**
```
sensor_processing.cpp:task_loop()
  → apply_moving_average() for each sensor
  → circular_buffer.cpp:write()
  → Data available in buffer
```

**Why Important:** Reduces sensor noise, which is critical for reliable control decisions. This demonstrates digital signal processing (DSP) in real-time systems.

**Related Topic:** Moving average filters, Producer role in Producer-Consumer pattern

---

### 2. Command Logic (State Machine)
**Command:** Automatically started
**Result:** Manages truck operational state (Manual/Automatic, Fault/OK)

**Code Flow:**
```
command_logic.cpp:task_loop()
  → circular_buffer.cpp:read() (Consumer)
  → check_faults() - Verify temperature > 120°C, electrical/hydraulic faults
  → process_commands() - Handle mode switches, rearm
  → calculate_actuator_outputs()
  → Update state available via get_state()
```

**Why Important:** Central control logic that ensures safe operation. Fault states override all other commands, preventing dangerous operations.

**Related Topic:** State machines, finite state automata, safety-critical systems

---

### 3. Fault Monitoring
**Command:** Automatically started
**Result:** Detects and reports fault conditions

**Code Flow:**
```
fault_monitoring.cpp:task_loop()
  → circular_buffer.cpp:peek_latest() (non-consuming read)
  → check_for_faults()
    - Temperature > 120°C → CRITICAL
    - Temperature > 95°C → ALERT
    - Electrical/Hydraulic flags
  → notify_fault_event() via callbacks
```

**Why Important:** Early fault detection prevents equipment damage and ensures operator safety. Uses event-driven architecture for immediate response.

**Related Topic:** Event-driven systems, Observer pattern, fault tolerance

---

### 4. Navigation Control
**Command:** Automatically started
**Result:** Calculates acceleration and steering outputs

**Code Flow:**
```
navigation_control.cpp:task_loop()
  → Read truck_state (automatic mode?)
  → IF automatic:
      speed_controller() - Proportional control based on distance to target
      angle_controller() - Proportional control for heading
    ELSE (manual or fault):
      Bumpless transfer - Set setpoints = current values
  → get_output() provides ActuatorOutput
```

**Why Important:** Implements feedback control loops essential for autonomous navigation. Bumpless transfer prevents control jumps when switching modes.

**Related Topic:** Control systems, PID controllers, feedback loops, bumpless transfer

---

### 5. Route Planning
**Command:** `route_planner.set_target_waypoint(x, y, speed)`
**Result:** Provides navigation setpoints

**Code Flow:**
```
route_planning.cpp:set_target_waypoint()
  → Store target coordinates and speed
  → calculate_target_angle() uses atan2() for heading
  → get_setpoint() provides NavigationSetpoint
```

**Why Important:** Path planning is fundamental to autonomous systems. This simplified version will be enhanced in Stage 2 with Mine Management integration.

**Related Topic:** Path planning, trajectory generation, waypoint navigation

---

### 6. Data Collector
**Command:** Automatically logs every 1 second
**Result:** Creates `logs/truck_1_log.csv` with timestamped events

**Code Flow:**
```
data_collector.cpp:task_loop()
  → get_timestamp() - Milliseconds since epoch
  → Collect sensor data and truck state
  → log_event() writes CSV line
    Format: Timestamp,TruckID,State,PosX,PosY,Description
  → File I/O protected by mutex
```

**Example Log:**
```csv
Timestamp,TruckID,State,PositionX,PositionY,Description
1762729053595,1,MANUAL,0,0,Periodic status update
1762729054601,1,FAULT,100,200,Temperature critical fault detected
```

**Why Important:** Logging enables system diagnostics, performance analysis, and debugging. Essential for maintaining complex real-time systems.

**Related Topic:** Data persistence, system monitoring, diagnostics

---

### 7. Local Interface (HMI)
**Command:** Display updates automatically every 2 seconds
**Result:** Terminal UI showing truck status

**Code Flow:**
```
local_interface.cpp:task_loop()
  → peek_latest() for sensor data
  → display_status() shows:
    - Truck state (MODE, FAULT)
    - Sensor readings (Position, Heading, Temperature)
    - Actuator outputs (Acceleration, Steering)
  → ANSI escape codes clear screen
```

**Display Example:**
```
========================================
   AUTONOMOUS TRUCK - LOCAL INTERFACE
========================================

TRUCK STATE:
  Mode:        MANUAL
  Fault:       OK

SENSOR READINGS:
  Position:    (  100,   200)
  Heading:        45 degrees
  Temperature:    75 °C
  Electrical:  OK
  Hydraulic:   OK

ACTUATOR OUTPUTS:
  Acceleration:    0 %
  Steering:        0 degrees
```

**Why Important:** Human-machine interface (HMI) is critical for operator oversight and system transparency in automation.

**Related Topic:** HMI design, operator interfaces, situation awareness

---

## Code Flow and Architecture

### Overall System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Main Thread                           │
│  - Initializes all tasks                                     │
│  - Coordinates task communication                            │
│  - Handles SIGINT for graceful shutdown                      │
└─────────────────────────────────────────────────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
    ┌───▼────┐       ┌───────▼──────┐      ┌─────▼────┐
    │ Sensor │       │   Circular   │      │ Command  │
    │Process │──────▶│    Buffer    │◀─────│  Logic   │
    └────────┘       │  (200 slots) │      └──────────┘
   PRODUCER          └──────────────┘       CONSUMER
     100ms                MUTEX              50ms
                     CONDITION VARS
```

### Synchronization Mechanisms Used

| Mechanism | Where Used | Purpose |
|-----------|-----------|---------|
| **Mutex** (`std::mutex`) | Circular buffer, all shared state | Protect critical sections |
| **Condition Variables** (`std::condition_variable`) | Circular buffer | Signal buffer state changes (not_full, not_empty) |
| **Atomic** (`std::atomic<bool>`) | Task running flags | Thread-safe boolean flags |
| **Lock Guard** (`std::lock_guard`) | Most critical sections | RAII-based automatic mutex unlock |
| **Unique Lock** (`std::unique_lock`) | Condition variable waits | Flexible mutex locking |

---

## Real-Time Automation Concepts

### 1. Producer-Consumer Pattern (Bounded Buffer)
**Implementation:** Sensor Processing (Producer) writes to Circular Buffer, Command Logic (Consumer) reads from it.

**Why:** Classic synchronization problem from course. Prevents buffer overflow (producer blocking) and underflow (consumer blocking).

**Code Reference:**
- `circular_buffer.cpp:write()` - Producer operation with `not_full_.wait()`
- `circular_buffer.cpp:read()` - Consumer operation with `not_empty_.wait()`

---

### 2. Monitors and Condition Variables
**Implementation:** Circular buffer uses condition variables `not_full_` and `not_empty_`.

**Why:** Avoids busy-waiting. Threads sleep until notified, saving CPU resources.

**Code Reference:**
- `circular_buffer.cpp:21` - Wait predicate: `return count_ < BUFFER_SIZE`
- `circular_buffer.cpp:36` - Notify: `not_empty_.notify_one()`

---

### 3. Periodic Task Execution
**Implementation:** All tasks use `std::this_thread::sleep_until()` for precise timing.

**Why:** Real-time systems require predictable, periodic execution for control loops and monitoring.

**Code Reference:**
- `sensor_processing.cpp:54` - `next_execution += std::chrono::milliseconds(period_ms_)`
- Ensures consistent sampling rate regardless of computation time

---

### 4. State Machines
**Implementation:** Command Logic manages truck states with transitions based on events.

**States:**
- Manual/Automatic mode
- Fault/OK status

**Transitions:**
- Command (auto/manual)
- Fault detection
- Fault rearm

**Code Reference:**
- `command_logic.cpp:process_commands()` - Mode transitions
- `command_logic.cpp:calculate_actuator_outputs()` - State-dependent behavior

---

### 5. Event-Driven Architecture
**Implementation:** Fault Monitoring triggers callbacks when faults detected.

**Why:** Immediate response to critical events without polling.

**Code Reference:**
- `fault_monitoring.h:21` - `FaultCallback` type definition
- `fault_monitoring.cpp:110` - `notify_fault_event()` calls all registered callbacks

---

### 6. Resource Acquisition Is Initialization (RAII)
**Implementation:** `std::lock_guard` and `std::unique_lock` automatically release mutexes.

**Why:** Prevents deadlocks from forgotten unlocks or early returns.

**Code Reference:**
- `circular_buffer.cpp:16` - `std::unique_lock<std::mutex> lock(mutex_)`
- Lock automatically released when `lock` goes out of scope

---

## Troubleshooting

### Build Errors

**Problem:** `CMake not found`
**Solution:** Install CMake: `brew install cmake` (macOS) or `apt-get install cmake` (Linux)

**Problem:** `pthread not found`
**Solution:** Already included in most Unix systems. Ensure `-pthread` flag is in CMakeLists.txt (already configured).

---

### Runtime Issues

**Problem:** Program hangs/freezes
**Possible Cause:** Deadlock in synchronization
**Solution:** Check if all mutexes are properly released. Use `Ctrl+\` to force quit and check logs.

**Problem:** Log file not created
**Solution:** Ensure `logs/` directory exists. CMake should create it automatically. Run: `mkdir -p logs`

**Problem:** Segmentation fault
**Solution:** Check buffer initialization. Ensure all tasks are started before accessing shared data.

---

## Next Steps (Stage 2)

- **Mine Simulation GUI** (Python + pygame): Visual truck simulation with physics
- **Mine Management GUI** (Python): Multi-truck monitoring and control
- **MQTT Integration**: Pub/sub communication between C++ and Python
- **Interactive Keyboard Control**: Real-time operator commands
- **Performance Testing**: Measure task execution times under load

---

## Additional Resources

- **Course Content:** Real-Time Automation - Prof. Leandro Freitas
- **Synchronization Patterns:** Silberschatz, "Operating System Concepts"
- **Real-Time Systems:** Buttazzo, "Hard Real-Time Computing Systems"
- **MQTT Protocol:** https://mqtt.org/

---

**Project completed by:** ATR 2025.2
**Date:** November 2025
**Status:** Stage 1 Complete ✓
