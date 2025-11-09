# Stage 2 - Integration Guide

## Overview

Stage 2 adds Python GUIs and MQTT communication to create a complete distributed system for autonomous truck control.

## Architecture

```
┌─────────────────┐         ┌──────────────┐         ┌─────────────────┐
│  Mine           │         │    MQTT      │         │  Mine           │
│  Simulation     │◄────────┤    Broker    │────────►│  Management     │
│  (Python)       │         │  (Mosquitto) │         │  (Python)       │
└─────────────────┘         └──────────────┘         └─────────────────┘
        │                          │                          │
        │ Publishes                │                          │ Subscribes
        │ Sensor Data              │                          │ Truck Data
        │                          │                          │
        └──────────────────────────┼──────────────────────────┘
                                   │
                                   │ Commands
                                   │ Setpoints
                                   │
                           ┌───────▼────────┐
                           │   C++ Truck    │
                           │   Control      │
                           │   System       │
                           └────────────────┘
```

## Components

### 1. Mine Simulation (Python + Pygame)

**File:** `python_gui/mine_simulation.py`

**Features:**
- 2D visualization of mining environment
- Physics simulation with inertia and friction
- Random sensor noise generation
- Fault injection (E, H, T keys)
- MQTT publisher for sensor data
- MQTT subscriber for actuator commands

**MQTT Topics:**
- Publishes: `truck/1/sensors` (sensor data with noise)
- Subscribes: `truck/1/commands` (acceleration, steering)

**Controls:**
- SPACE: Pause/Resume simulation
- E: Toggle electrical fault
- H: Toggle hydraulic fault
- T: Increase temperature
- ESC: Quit

**Code Flow:**
```
mine_simulation.py:run()
  → update_physics() - Apply acceleration, steering, friction
  → get_sensor_data_with_noise() - Add random noise
  → publish_sensor_data() - Send via MQTT every 10 frames
  → on_mqtt_message() - Receive actuator commands
```

**Why Important:** Simulates the real-world physics and sensor characteristics that the control system must handle. Demonstrates simulation-based testing.

---

### 2. Mine Management (Python + Tkinter)

**File:** `python_gui/mine_management.py`

**Features:**
- Real-time map showing all trucks
- Position trail visualization
- Waypoint setting (click map or enter coordinates)
- Mode control (Manual/Auto)
- Fault rearm
- Live telemetry display

**MQTT Topics:**
- Subscribes: `truck/+/sensors`, `truck/+/state`
- Publishes: `truck/{id}/commands`, `truck/{id}/setpoint`

**Controls:**
- Click map to set waypoint
- "Send Waypoint" button
- "Manual Mode" / "Auto Mode" buttons
- "Rearm Fault" button

**Code Flow:**
```
mine_management.py:on_mqtt_message()
  → Parse truck_id from topic
  → Update TruckData object
  → draw_trucks() - Render on map
  → update_info_panel() - Show telemetry

User clicks "Auto Mode":
  → send_mode_command(True)
  → Publish to truck/1/commands
  → {"auto_mode": true}
```

**Why Important:** Demonstrates supervisory control (SCADA) concepts. Shows how operators monitor and control distributed autonomous systems.

---

### 3. MQTT Broker (Mosquitto)

**Installation:**
- macOS: `brew install mosquitto`
- Linux: `sudo apt-get install mosquitto`

**Running:**
```bash
mosquitto
```

**Why Important:** Central message broker for pub/sub communication. Decouples components - simulation doesn't need to know about management GUI.

---

### 4. C++ Truck Control (Enhanced for Stage 2)

The Stage 1 C++ system continues to run, managing:
- Sensor processing
- Command logic
- Fault monitoring
- Navigation control
- Data logging

For Stage 2 integration:
- Reads sensor data from MQTT (via bridge or direct)
- Publishes truck state to MQTT
- Receives commands/setpoints from MQTT

---

## Installation and Setup

### Step 1: Install Dependencies

```bash
# Run setup script
./setup_stage2.sh
```

Or manually:

```bash
# Install Python packages
pip3 install -r requirements.txt

# Install MQTT broker
# macOS:
brew install mosquitto

# Linux:
sudo apt-get install mosquitto mosquitto-clients

# Build C++ project
mkdir -p build
cd build
cmake ..
make
cd ..
```

### Step 2: Verify Installation

```bash
# Check Python packages
python3 -c "import pygame, paho.mqtt.client; print('OK')"

# Check Mosquitto
mosquitto -h | head -1

# Check C++ build
ls -la build/truck_control
```

---

## Running the System

### Option 1: Automated (Recommended)

```bash
./run_stage2.sh
```

This launches all components automatically.

### Option 2: Manual (For Debugging)

Open **4 terminal windows**:

**Terminal 1 - MQTT Broker:**
```bash
mosquitto
```

**Terminal 2 - Mine Simulation:**
```bash
python3 python_gui/mine_simulation.py
```

**Terminal 3 - Mine Management:**
```bash
python3 python_gui/mine_management.py
```

**Terminal 4 - C++ Truck Control:**
```bash
./build/truck_control
```

---

## Testing Scenarios

### Scenario 1: Basic Operation
1. Start all components
2. In Mine Simulation, observe truck at position (100, 200)
3. In Mine Management, truck should appear on map
4. Click "Auto Mode" in Mine Management
5. Click map to set waypoint
6. Click "Send Waypoint"
7. Observe truck moving towards waypoint in simulation

### Scenario 2: Fault Handling
1. In Mine Simulation, press 'E' (electrical fault)
2. Truck color changes to RED
3. In Mine Management, fault indicator shows "FAULT"
4. Truck stops (fault state)
5. Click "Rearm Fault" in Mine Management
6. Press 'E' again to clear fault
7. System returns to normal operation

### Scenario 3: Temperature Monitoring
1. Press 'T' multiple times in simulation
2. Temperature increases above 95°C → Orange color
3. Temperature above 120°C → RED color, CRITICAL
4. Management GUI shows warning
5. Data Collector logs temperature fault
6. Fault must be rearmed to continue

---

## MQTT Message Format

### Sensor Data (Simulation → All)
**Topic:** `truck/1/sensors`
```json
{
  "truck_id": 1,
  "position_x": 100,
  "position_y": 200,
  "angle_x": 45,
  "temperature": 85,
  "fault_electrical": false,
  "fault_hydraulic": false,
  "timestamp": 1762729053595
}
```

### Truck State (C++ → All)
**Topic:** `truck/1/state`
```json
{
  "automatic": false,
  "fault": false
}
```

### Commands (Management → C++)
**Topic:** `truck/1/commands`
```json
{
  "auto_mode": true,
  "manual_mode": false,
  "rearm": false
}
```

### Setpoint (Management → C++)
**Topic:** `truck/1/setpoint`
```json
{
  "target_x": 500,
  "target_y": 300,
  "target_speed": 50
}
```

---

## Real-Time Automation Concepts (Stage 2)

### MQTT Pub/Sub Pattern
**Why:** Decouples publishers from subscribers. New components can be added without modifying existing code.

**Example:** Mine Simulation publishes sensor data. Both Management GUI and C++ Control subscribe. Adding a data logger is trivial - just subscribe to the topic.

### Distributed Systems
**Why:** Mirrors real industrial systems where HMI, controllers, and simulations run on separate machines.

**Code:** Each component runs independently, communicating only via MQTT.

### SCADA (Supervisory Control and Data Acquisition)
**Why:** Essential pattern in industrial automation. Operators need high-level oversight of distributed equipment.

**Implementation:** Mine Management GUI = SCADA system, Trucks = field devices

### Event-Driven Communication
**Why:** Efficient - components react to data/events rather than polling.

**Code:** `on_mqtt_message()` callbacks triggered when data arrives

### Quality of Service (QoS)
**MQTT QoS Levels:**
- QoS 0: At most once (fire and forget)
- QoS 1: At least once (acknowledged)
- QoS 2: Exactly once (guaranteed)

**Current:** Using QoS 0 for simplicity. Production systems would use QoS 1/2 for critical commands.

---

## Troubleshooting

**Problem:** `ModuleNotFoundError: No module named 'paho'`
**Solution:** `pip3 install paho-mqtt`

**Problem:** `ModuleNotFoundError: No module named 'pygame'`
**Solution:** `pip3 install pygame`

**Problem:** MQTT connection refused
**Solution:** Ensure Mosquitto is running: `mosquitto` in separate terminal

**Problem:** Simulation window doesn't appear
**Solution:** Check if pygame installed correctly. On macOS, may need: `brew install sdl2 sdl2_image sdl2_ttf`

**Problem:** Components can't connect to MQTT
**Solution:** Check firewall settings. MQTT uses port 1883.

---

## Performance Notes

- **Simulation FPS:** 30 (can be adjusted in code)
- **Sensor Publish Rate:** ~3 Hz (every 10 frames)
- **Management Update Rate:** 10 Hz (100ms)
- **C++ Task Periods:** 50-1000ms (unchanged from Stage 1)

---

## Next Steps

With Stage 2 complete, you have a full distributed real-time system demonstrating:
- ✅ Concurrent programming (Stage 1)
- ✅ Synchronization mechanisms (Stage 1)
- ✅ Producer-Consumer pattern (Stage 1)
- ✅ State machines (Stage 1)
- ✅ Control loops (Stage 1)
- ✅ MQTT pub/sub (Stage 2)
- ✅ Distributed systems (Stage 2)
- ✅ SCADA concepts (Stage 2)
- ✅ Simulation and testing (Stage 2)

**Potential Enhancements:**
- Multi-truck support (add more trucks to simulation)
- Path planning algorithms (A*, Dijkstra)
- Collision avoidance between trucks
- Battery/fuel management
- Advanced PID controllers
- Network latency simulation
- Database integration for historical data
- Web-based dashboard

---

**Stage 2 Complete!** 🎉
