#!/bin/bash
# Setup script for Stage 2 - MQTT and Python GUIs

echo "========================================="
echo "Stage 2 Setup Script"
echo "========================================="
echo ""

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is not installed"
    echo "Please install Python 3 first"
    exit 1
fi

echo "[1/4] Checking Python version..."
python3 --version

# Install Python dependencies
echo ""
echo "[2/4] Installing Python dependencies..."
pip3 install --user -r requirements.txt
if [ $? -eq 0 ]; then
    echo "✓ Python packages installed successfully"
else
    echo "⚠ Warning: Some packages may not have installed correctly"
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
echo "Setup Complete!"
echo "========================================="
echo ""
echo "To run Stage 2 system:"
echo ""
echo "Terminal 1: mosquitto"
echo "Terminal 2: python3 python_gui/mine_simulation.py"
echo "Terminal 3: python3 python_gui/mine_management.py"
echo "Terminal 4: ./build/truck_control"
echo ""
echo "Or use: ./run_stage2.sh"
echo "========================================="
