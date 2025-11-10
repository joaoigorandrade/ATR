#!/bin/bash
# Run script for Stage 2 - Launches all components

echo "========================================="
echo "Starting Stage 2 System"
echo "========================================="
echo ""

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

# Check if build exists
if [ ! -f "build/truck_control" ]; then
    echo "Error: C++ project not built. Run ./setup_stage2.sh first"
    exit 1
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

# Wait and handle shutdown
trap "echo ''; echo 'Shutting down...'; kill $BRIDGE_PID $SIM_PID $MGMT_PID $TRUCK_PID 2>/dev/null; wait $BRIDGE_PID $SIM_PID $MGMT_PID $TRUCK_PID 2>/dev/null; echo 'All components stopped'; exit 0" INT TERM

# Wait for any process to exit
wait $BRIDGE_PID $SIM_PID $MGMT_PID $TRUCK_PID

echo ""
echo "System stopped"
