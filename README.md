# Autonomous Mining Truck Control System

Real-time automation project implementing an autonomous truck control system for mining operations, demonstrating concurrent programming, synchronization mechanisms, and real-time task coordination.

## Project Overview

This system implements a simplified control system for autonomous mining trucks, covering:
- **Stage 1 (COMPLETE):** Core C++ tasks with circular buffer synchronization
- **Stage 2 (PENDING):** Python GUIs (Mine Simulation, Mine Management) and MQTT communication

## Key Features

✅ **Implemented (Stage 1):**
- Thread-safe circular buffer (Producer-Consumer pattern)
- Sensor Processing task with moving average filter
- Command Logic state machine (Manual/Auto modes)
- Fault Monitoring with event-driven architecture
- Navigation Control with P controllers
- Route Planning for waypoint navigation
- Data Collector with CSV logging
- Local Interface (Terminal-based HMI)

⏳ **Pending (Stage 2):**
- Mine Simulation GUI (Python + pygame)
- Mine Management GUI (Python)
- MQTT broker and pub/sub integration
- Multi-truck support

## Project Structure

```
.
├── src/              # C++ source files (all 7 core tasks)
├── include/          # C++ header files
├── build/            # Build output directory
├── logs/             # Runtime event logs (CSV format)
├── python_gui/       # Python GUI applications (Stage 2)
├── doc/              # Project specification documents
├── CMakeLists.txt    # Build configuration
├── README.md         # This file
└── USER_GUIDE.md     # Comprehensive user and technical guide
```

## Quick Start

### Prerequisites
- **C++ Compiler:** g++ or clang++ with C++17 support
- **CMake:** Version 3.14 or higher
- **pthread:** Included in most Unix systems (Linux, macOS)

### Build and Run

```bash
# From project root directory
mkdir -p build
cd build
cmake ..
make

# Run the system
cd ..
./build/truck_control

# Stop with Ctrl+C for graceful shutdown
```

### What You'll See

The system will:
1. Initialize all 7 concurrent tasks
2. Display real-time truck status on terminal (updates every 2s)
3. Log events to `logs/truck_1_log.csv`
4. Demonstrate Producer-Consumer pattern, state machines, and control loops

Example terminal output:
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

## Documentation

📘 **[USER_GUIDE.md](USER_GUIDE.md)** - Comprehensive guide including:
- Detailed command reference
- Code flow diagrams
- Real-time automation concepts explained
- Troubleshooting

📄 **doc/** directory - Project specifications and requirements

## Real-Time Automation Concepts Demonstrated

| Concept | Implementation |
|---------|---------------|
| **Producer-Consumer Pattern** | Sensor Processing → Circular Buffer → Command Logic |
| **Mutex & Condition Variables** | Thread-safe buffer access, event notification |
| **State Machines** | Command Logic manages truck modes and fault states |
| **Periodic Tasks** | All tasks execute at fixed intervals (50-1000ms) |
| **Event-Driven Architecture** | Fault Monitoring with callback notifications |
| **RAII Pattern** | Automatic mutex unlock with lock_guard/unique_lock |
| **Control Loops** | Navigation Control with proportional controllers |
| **Data Logging** | CSV-based event logging with timestamps |

## Development Status

- [x] **Stage 1 Core Implementation** - COMPLETE
  - [x] Circular buffer with synchronization
  - [x] All 7 blue tasks from specification
  - [x] Producer-Consumer pattern
  - [x] Mutex and condition variable usage
  - [x] Periodic task execution
  - [x] Event-driven fault handling
  - [x] Data logging
  - [x] Terminal-based interface
  - [x] Comprehensive documentation

- [ ] **Stage 2 Integration** - PENDING
  - [ ] Mine Simulation GUI (Python + pygame)
  - [ ] Mine Management GUI (Python)
  - [ ] MQTT broker setup
  - [ ] Publisher/Subscriber implementation
  - [ ] Multi-truck support
  - [ ] Performance testing

## Architecture

```
Main Thread
    │
    ├─► Sensor Processing (100ms) ────► Circular Buffer ──┐
    │                                    (200 slots)        │
    ├─► Command Logic (50ms) ◄──────────────────────────────┘
    │        │
    │        ├─► State Machine (Manual/Auto, Fault/OK)
    │        └─► Actuator Outputs
    │
    ├─► Fault Monitoring (100ms) ───► Event Callbacks
    │
    ├─► Navigation Control (50ms) ───► P Controllers
    │
    ├─► Route Planning ──────────────► Setpoints
    │
    ├─► Data Collector (1000ms) ─────► CSV Logs
    │
    └─► Local Interface (2000ms) ────► Terminal Display
```

## Testing

The system has been tested for:
- ✅ Task startup and graceful shutdown
- ✅ Circular buffer thread safety (no race conditions)
- ✅ Producer-Consumer synchronization
- ✅ Fault detection and logging
- ✅ State transitions
- ✅ Data persistence (CSV logs)

## Troubleshooting

**Build fails?**
- Ensure CMake 3.14+ is installed
- Check C++17 compiler support
- Verify pthread availability

**Program hangs?**
- Use Ctrl+C for graceful shutdown
- Check `logs/` for error messages

**See [USER_GUIDE.md](USER_GUIDE.md) for detailed troubleshooting.**

## Author

**Course:** Real-Time Automation (ATR)
**Semester:** 2025.2
**Professor:** Leandro Freitas

## License

Academic project for educational purposes.
