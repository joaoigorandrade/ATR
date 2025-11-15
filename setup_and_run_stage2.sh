#!/bin/bash
# Combined setup and run script for Stage 2 - Builds and runs the complete system

echo "========================================="
echo "Stage 2 Setup and Run Script"
echo "========================================="
echo ""

# Function to cleanup on exit
cleanup() {
    echo ""
    echo "Shutting down..."
    if [ ! -z "$BRIDGE_PID" ]; then kill $BRIDGE_PID 2>/dev/null; fi
    if [ ! -z "$SIM_PID" ]; then kill $SIM_PID 2>/dev/null; fi
    if [ ! -z "$MGMT_PID" ]; then kill $MGMT_PID 2>/dev/null; fi
    if [ ! -z "$TRUCK_PID" ]; then kill $TRUCK_PID 2>/dev/null; fi
    echo "All components stopped"
    exit 0
}

trap cleanup INT TERM

# ===== SETUP PHASE =====
echo "=== SETUP PHASE ==="
echo ""

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is not installed"
    echo "Please install Python 3 first"
    exit 1
fi

echo "[1/4] Checking Python version..."
python3 --version

# Install Python dependencies (tkinter usually comes with Python, so install others first)
echo ""
echo "[2/4] Installing Python dependencies..."
pip3 install --user paho-mqtt pygame
if [ $? -eq 0 ]; then
    echo "✓ Python packages installed successfully"
else
    echo "⚠ Warning: Some packages may not have installed correctly"
fi

# Check for tkinter availability
python3 -c "import tkinter" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✓ tkinter available"
else
    echo "⚠ tkinter not available - Mine Management GUI will use alternative interface"
fi

# Check for MQTT broker (Mosquitto)
echo ""
echo "[3/4] Checking for MQTT broker (Mosquitto)..."
if command -v mosquitto &> /dev/null; then
    echo "✓ Mosquitto is installed"
    mosquitto -h | head -1
else
    echo "⚠ Mosquitto MQTT broker not found"
    echo ""
    echo "To install Mosquitto:"
    echo "  macOS:   brew install mosquitto"
    echo "  Linux:   sudo apt-get install mosquitto mosquitto-clients"
    echo ""
    echo "You can continue without Mosquitto, but MQTT features won't work"
fi

# Build C++ project
echo ""
echo "[4/4] Building C++ project..."
mkdir -p build
cd build
cmake .. > /dev/null 2>&1
make > /dev/null 2>&1
cd ..

if [ -f "build/truck_control" ]; then
    echo "✓ C++ project built successfully"
else
    echo "✗ C++ build failed"
    exit 1
fi

echo ""
echo "========================================="
echo "Setup Complete! Starting System..."
echo "========================================="
echo ""

# ===== RUN PHASE =====

# Check if mosquitto is running
if ! pgrep -x "mosquitto" > /dev/null; then
    echo "[MQTT] Starting Mosquitto broker..."
    mosquitto -d > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "✓ Mosquitto started"
    else
        echo "⚠ Could not start Mosquitto (may already be running or not installed)"
    fi
    sleep 1
else
    echo "✓ Mosquitto already running"
fi

echo ""
echo "Launching components..."
echo "========================================="
echo ""
echo "Starting in separate terminal windows..."
echo ""
echo "1. MQTT Bridge (background)"
echo "2. Mine Simulation (Pygame window)"
echo "3. Mine Management (Tkinter window)"
echo "4. C++ Truck Control (Terminal)"
echo ""
echo "Press Ctrl+C in this terminal to stop all components"
echo "========================================="
echo ""

# Launch MQTT bridge first
python3 python_gui/mqtt_bridge.py &
BRIDGE_PID=$!
echo "[Started] MQTT Bridge (PID: $BRIDGE_PID)"

sleep 2

# Launch components in background
python3 python_gui/mine_simulation.py &
SIM_PID=$!
echo "[Started] Mine Simulation (PID: $SIM_PID)"

sleep 2

python3 python_gui/mine_management.py &
MGMT_PID=$!
echo "[Started] Mine Management (PID: $MGMT_PID)"

sleep 2

./build/truck_control &
TRUCK_PID=$!
echo "[Started] Truck Control (PID: $TRUCK_PID)"

echo ""
echo "All components running!"
echo ""
echo "To stop all components, press Ctrl+C"
echo ""

# Wait for any process to exit
wait $BRIDGE_PID $SIM_PID $MGMT_PID $TRUCK_PID 2>/dev/null

echo ""
echo "System stopped"
