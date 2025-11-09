# Autonomous Mining Truck Control System

Real-time automation project for controlling autonomous mining trucks with concurrent tasks and MQTT communication.

## Project Structure

```
.
├── src/              # C++ source files
├── include/          # C++ header files
├── build/            # Build output directory
├── logs/             # Runtime log files
├── python_gui/       # Python GUI applications (Stage 2)
├── doc/              # Project documentation
└── CMakeLists.txt    # Build configuration
```

## Build Instructions

### Prerequisites
- C++ compiler with C++17 support (g++ or clang++)
- CMake 3.14 or higher
- pthread library (included in most Unix systems)

### Building the Project

```bash
# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Run the executable
./truck_control
```

## Development Status

- [x] Stage 1: Core C++ tasks with circular buffer
- [ ] Stage 2: Python GUIs and MQTT communication

## Author

ATR Course Project - 2025.2
