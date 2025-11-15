# Lock Ordering and Deadlock Prevention

## Overview

This document defines the global lock ordering strategy for the Autonomous Mining Truck Control System to prevent deadlocks. All code must follow this ordering when acquiring multiple locks.

## Real-Time Automation Concepts

**Deadlock Prevention (Coffman Conditions)**:
1. **Mutual Exclusion**: Cannot be avoided (mutexes are required)
2. **Hold and Wait**: Prevented by acquiring all locks atomically
3. **No Preemption**: Cannot be avoided in userspace
4. **Circular Wait**: **PREVENTED by enforcing lock ordering**

## Global Lock Hierarchy

Locks must be acquired in this order (top to bottom):

```
Level 1: CircularBuffer::mutex_              (Highest - data producer)
Level 2: SensorProcessing::raw_data_mutex_   (Sensor input)
Level 3: FaultMonitoring::fault_mutex_       (Fault state)
Level 4: CommandLogic::state_mutex_          (Truck state)
Level 5: NavigationControl::control_mutex_   (Control outputs)
Level 6: FaultMonitoring::callback_mutex_    (Lowest - callbacks)
```

## Rules

### Rule 1: Single Lock Acquisition
When acquiring **only one lock**, use `std::lock_guard`:
```cpp
std::lock_guard<std::mutex> lock(state_mutex_);
```

### Rule 2: Multiple Lock Acquisition
When acquiring **multiple locks**, use `std::scoped_lock` (C++17):
```cpp
// Automatically acquires locks in safe order to prevent deadlock
std::scoped_lock lock(buffer_mutex_, state_mutex_);
```

### Rule 3: Lock Ordering Enforcement
Never acquire locks in reverse order:
```cpp
// ❌ WRONG - Potential deadlock!
std::lock_guard<std::mutex> lock1(control_mutex_);  // Level 5
std::lock_guard<std::mutex> lock2(state_mutex_);    // Level 4 (lower)

// ✅ CORRECT - Follows hierarchy
std::scoped_lock lock(state_mutex_, control_mutex_);
```

### Rule 4: Condition Variable Usage
When using condition variables, use `std::unique_lock`:
```cpp
std::unique_lock<std::mutex> lock(mutex_);
condition_var_.wait(lock, [this]() { return ready_; });
```

## Current Lock Usage by Task

### Sensor Processing
- **Locks**: `raw_data_mutex_` (Level 2)
- **Risk**: Low (single lock only)
- **Pattern**: `std::lock_guard`

### Circular Buffer
- **Locks**: `mutex_` (Level 1)
- **Risk**: Medium (accessed by all tasks)
- **Pattern**: `std::lock_guard` or `std::unique_lock` for CVs

### Fault Monitoring
- **Locks**: `fault_mutex_` (Level 3), `callback_mutex_` (Level 6)
- **Risk**: Medium (callback execution)
- **Pattern**: Acquire `fault_mutex_` before `callback_mutex_`

### Command Logic
- **Locks**: `state_mutex_` (Level 4)
- **Risk**: High (reads from buffer, interacts with navigation)
- **Pattern**: `std::lock_guard` for single lock

### Navigation Control
- **Locks**: `control_mutex_` (Level 5)
- **Risk**: Low (single lock only)
- **Pattern**: `std::lock_guard`

## Deadlock Prevention Strategies Employed

### 1. Lock Ordering (Primary)
- Global hierarchy prevents circular wait
- Documented in this file
- Enforced through code review

### 2. Atomic Lock Acquisition
- Use `std::scoped_lock` for multiple locks
- Prevents hold-and-wait condition
- C++17 feature (deadlock-free)

### 3. Lock-Free Reads Where Possible
- Use `peek_latest()` instead of `read()` for non-blocking buffer access
- Reduces lock contention
- Minimizes lock hold time

### 4. RAII Pattern
- Always use `std::lock_guard` or `std::scoped_lock`
- Automatic unlock prevents forgetting to release
- Exception-safe lock management

## Testing for Deadlocks

### Static Analysis
```bash
# Use ThreadSanitizer to detect deadlocks
g++ -fsanitize=thread -g -O1 src/*.cpp -o truck_control
./truck_control
```

### Runtime Detection
- Enable logging of lock acquisitions (DEBUG level)
- Monitor for hangs during testing
- Use `gdb` to inspect thread states if hang occurs

### Stress Testing
```bash
# Run for extended period under high load
while true; do
    timeout 60 ./truck_control
    if [ $? -eq 124 ]; then
        echo "Deadlock detected (timeout)"
        break
    fi
done
```

## Examples

### Safe Pattern: Single Lock
```cpp
void CommandLogic::get_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_state_;
}
```

### Safe Pattern: Multiple Locks
```cpp
void update_state_with_sensor() {
    // buffer_mutex_ (Level 1) acquired before state_mutex_ (Level 4)
    std::scoped_lock lock(buffer_.get_mutex(), state_mutex_);
    // Safe: follows hierarchy
}
```

### Unsafe Pattern (Avoided)
```cpp
// ❌ DON'T DO THIS - Deadlock risk
void dangerous_function() {
    std::lock_guard<std::mutex> lock1(state_mutex_);     // Level 4
    std::lock_guard<std::mutex> lock2(fault_mutex_);     // Level 3 - WRONG ORDER!
}
```

## Future Improvements

1. **Lock-Free Data Structures**: Replace mutexes with atomic operations where possible
2. **Reader-Writer Locks**: Use `std::shared_mutex` for buffer reads (see TIER 2 improvements)
3. **Lock Contention Profiling**: Measure lock hold times and identify bottlenecks
4. **Timeout-Based Deadlock Detection**: Use `std::timed_mutex` with automatic recovery

## References

- Coffman et al. (1971): "System Deadlocks" - Four necessary conditions
- Herlihy & Shavit (2012): "The Art of Multiprocessor Programming"
- C++17 Standard: std::scoped_lock specification
- Real-Time Automation Course: Section 6 - Classic Synchronization Problems

---

**Last Updated**: November 2025
**Status**: Implemented in TIER 1 improvements
