# Project: Autonomous Mining Truck Control System

## Project Overview
This project implements a real-time control and monitoring system for an autonomous mining truck. It integrates onboard truck control (C++) with a central mine management system (Python GUI) and a mine simulation (Python GUI) through an MQTT-based communication bridge (Python). The system focuses on concurrent programming, real-time data processing, fault tolerance, navigation control, and supervisory management, essential for Industry 4.0 applications in the mining sector.

## Technologies Used
*   **C++17:** Core truck control logic, sensor processing, command logic, navigation, fault monitoring, data collection, local interface, watchdog, and performance monitoring. Uses `pthread` for concurrency.
*   **Python 3:**
    *   **`paho-mqtt`:** MQTT client library for inter-component communication.
    *   **`pygame`:** Used for the Mine Simulation GUI.
    *   **`tkinter`:** Used for the Mine Management GUI.
    *   **`python-dateutil`:** Additional utility.
*   **CMake:** Build system for the C++ components.
*   **Mosquitto:** MQTT broker (external dependency).
*   **JSON:** Data exchange format between components (via files for the C++ application and directly via MQTT for Python components).

## Architecture
The system is divided into several interconnected components:

1.  **C++ Truck Control (`truck_control`):**
    *   **Core Tasks:** Sensor Processing, Command Logic, Fault Monitoring, Navigation Control, Data Collector, Local Interface. These tasks run as concurrent threads, managed by a `Watchdog` and monitored by a `PerformanceMonitor`.
    *   **Data Flow:** Uses a `CircularBuffer` for shared sensor data. Communicates with the Python MQTT bridge by reading/writing JSON files in `bridge/from_mqtt` and `bridge/to_mqtt` directories.
2.  **Python MQTT Bridge (`mqtt_bridge.py`):**
    *   Acts as an intermediary, publishing data from the C++ `bridge/to_mqtt` files to MQTT topics and writing data from MQTT topics to `bridge/from_mqtt` files for the C++ application to consume.
3.  **Python Mine Simulation GUI (`mine_simulation.py`):**
    *   Simulates sensor data and potentially allows injecting faults, publishing data to MQTT.
4.  **Python Mine Management GUI (`mine_management.py`):**
    *   Provides a supervisory interface to monitor all trucks in real-time.
    *   Displays truck positions, states, and fault information.
    *   Allows operators to set navigation waypoints and send control commands (e.g., mode changes, fault rearm) via MQTT.
5.  **MQTT Broker (Mosquitto):**
    *   Central message broker facilitating real-time communication between all Python and C++ components.

## Building and Running

**Prerequisites:**
*   Python 3 and `pip3`
*   `paho-mqtt` (Python package)
*   `pygame` (Python package)
*   `tkinter` (usually comes with Python)
*   `python-dateutil` (Python package)
*   `mosquitto` MQTT broker
*   `cmake`
*   `make`
*   C++ compiler (supporting C++17)

**Setup and Run (using provided script):**
The `setup_and_run_stage2.sh` script automates the setup and execution of the entire system.

1.  **Execute the script:**
    ```bash
    ./setup_and_run_stage2.sh
    ```
2.  **Script Actions:**
    *   Installs Python dependencies (`paho-mqtt`, `pygame`) if not already installed.
    *   Checks for `tkinter` and `mosquitto`. If `mosquitto` is not found, it provides installation instructions.
    *   Builds the C++ `truck_control` executable in the `build/` directory using `cmake` and `make`.
    *   Starts the `mosquitto` broker if it's not already running.
    *   Launches the `mqtt_bridge.py`, `mine_simulation.py`, `mine_management.py`, and `./build/truck_control` components in separate background processes.
3.  **Stopping the system:**
    *   Press `Ctrl+C` in the terminal where `setup_and_run_stage2.sh` is running to stop all launched components.

**Manual Build (C++ component only):**

```bash
mkdir -p build
cd build
cmake ..
make
cd ..
```
The executable `truck_control` will be generated in the `build/` directory.

## Development Conventions
*   **C++:** Uses C++17 standard with `-Wall -Wextra -pthread` compiler flags.
*   **Logging:** A custom `Logger` class is used for structured logging within the C++ application.
*   **Performance Monitoring:** A `PerformanceMonitor` tracks task execution times in the C++ application.
*   **Inter-process Communication (IPC):** Primarily relies on MQTT (via the Python bridge) and file-based communication (JSON files in `bridge/from_mqtt` and `bridge/to_mqtt` for C++ interaction).
*   **Python GUIs:** Developed using `pygame` for simulation and `tkinter` for central management.
*   **Error Handling:** C++ components often silently ignore bridge-related errors during file I/O, assuming the bridge might not be ready.
*   **Concurrency:** C++ tasks are implemented as separate threads, managed by a watchdog.
