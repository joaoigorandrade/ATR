# Performance Monitoring System

## Overview

The Performance Monitoring System provides comprehensive real-time execution time measurement and analysis for all periodic tasks in the autonomous mining truck control system. This feature is critical for validating real-time constraints and identifying performance bottlenecks.

## Real-Time Automation Concepts

This implementation covers several key real-time system concepts:

- **WCET (Worst-Case Execution Time)**: Tracks maximum observed execution time
- **Deadline Monitoring**: Detects when tasks exceed their period deadlines
- **Jitter Analysis**: Measures execution time variability (standard deviation)
- **CPU Utilization**: Calculates percentage of time spent executing vs. available time
- **Performance Profiling**: Statistical analysis over sliding window (last 100 samples)

## Architecture

### Components

**1. PerformanceMonitor Class** (`include/performance_monitor.h`, `src/performance_monitor.cpp`)
- Central monitoring system that tracks statistics for all registered tasks
- Thread-safe with mutex protection
- Provides registration, measurement, and reporting APIs

**2. PerformanceMeasurement RAII Helper**
- Automatic timing using constructor/destructor pattern
- Ensures measurements are recorded even on early returns
- Minimal overhead (just timestamp capture)

**3. Task Integration**
- All periodic tasks (SensorProcessing, CommandLogic, FaultMonitoring, NavigationControl, DataCollector, LocalInterface) instrumented
- Non-intrusive measurement (no algorithm changes)
- Optional feature (nullptr performance monitor supported)

## Key Features

### 1. Statistical Tracking

For each task, the system tracks:

```cpp
struct TaskStats {
    long current_execution_us;    // Last execution time
    long min_execution_us;         // Best case (minimum)
    long max_execution_us;         // WCET estimate (maximum)
    double avg_execution_us;       // Average execution time
    double std_dev_us;             // Jitter (standard deviation)
    int deadline_violations;       // Count of missed deadlines
    long worst_overrun_us;         // Worst deadline overrun
};
```

### 2. Deadline Violation Detection

The system automatically detects when a task's execution time exceeds its period:

- **Warning Level**: 80% of period → high utilization warning
- **Critical Level**: 100% of period → deadline miss logged

Example log output:
```
1763238874019|WARN|MAIN|task=SensorProcessing,exec_us=120000,deadline_us=100000,overrun_us=20000,event=deadline_miss
```

### 3. Performance Report

On system shutdown (Ctrl+C), a formatted performance report is displayed:

```
========================================
    TASK PERFORMANCE REPORT
========================================

Task                Period    Current     Min         Avg         Max         Std Dev     Util%     Violations
--------------------------------------------------------------------------------------------------------------
SensorProcessing    100ms     14μs       4μs        16μs       65μs       10μs       0.0       0
CommandLogic        50ms      36μs       1μs        10μs       84μs       14μs       0.0       0
FaultMonitoring     100ms     5μs        1μs        13μs       94μs       19μs       0.0       0
NavigationControl   50ms      52μs       1μs        9μs        64μs       13μs       0.0       0
DataCollector       1000ms    157μs      70μs       130μs      157μs      31μs       0.0       0
LocalInterface      2000ms    111μs      111μs      126μs      156μs      20μs       0.0       0
--------------------------------------------------------------------------------------------------------------

Summary:
  Total Tasks: 6
  Total Deadline Violations: 0
  ✓ All tasks meeting deadlines
========================================
```

### 4. Runtime Logging

Performance warnings logged during execution:

- **Deadline violations**: When execution time > period
- **High utilization**: When execution time > 80% of period
- **Auto-registration**: When unregistered task measured

## Usage

### 1. Register Tasks

In `main.cpp`, create and configure the performance monitor:

```cpp
PerformanceMonitor perf_monitor;

// Register tasks with expected periods
perf_monitor.register_task("SensorProcessing", 100);
perf_monitor.register_task("CommandLogic", 50);
perf_monitor.register_task("FaultMonitoring", 100);
perf_monitor.register_task("NavigationControl", 50);
perf_monitor.register_task("DataCollector", 1000);
perf_monitor.register_task("LocalInterface", 2000);
```

### 2. Pass to Tasks

Pass the performance monitor pointer to each task:

```cpp
SensorProcessing sensor_task(buffer, 5, 100, &perf_monitor);
CommandLogic command_task(buffer, 50, &perf_monitor);
// ... etc
```

### 3. Instrument Task Loops

In each task's `task_loop()` method:

```cpp
void SensorProcessing::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {
        // Start measurement
        auto start_time = std::chrono::steady_clock::now();

        // ... do work ...

        // End measurement
        if (perf_monitor_) {
            perf_monitor_->end_measurement("SensorProcessing", start_time);
        }

        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}
```

### 4. View Reports

**During Runtime:**
- Watch logs for deadline violations and high utilization warnings
- Use `perf_monitor.get_stats("TaskName")` for real-time access

**On Shutdown:**
- Press Ctrl+C to trigger graceful shutdown
- Performance report automatically printed to console

## Performance Metrics Interpretation

### Execution Time Statistics

**Current**: Most recent execution time
- Use: Spot-check current system load

**Min**: Shortest observed execution time
- Use: Best-case performance baseline
- Typically occurs when buffers are empty, no faults

**Avg**: Average execution time
- Use: Expected performance under normal load
- Most important for utilization calculations

**Max**: Longest observed execution time
- Use: WCET estimate (approximation)
- Important for hard real-time guarantees
- Should be < period for deadline compliance

**Std Dev**: Standard deviation (jitter)
- Use: Measure execution time predictability
- Low jitter = predictable, high jitter = variable
- Important for QoS and determinism

### CPU Utilization

Formula: `Util% = (Avg Execution Time / Period) × 100`

Example:
- SensorProcessing: 16μs avg / 100ms period = 0.016% utilization

**Interpretation:**
- < 50%: Comfortable margin
- 50-70%: Moderate load
- 70-90%: High load
- > 90%: Approaching deadline violations

**Total System Utilization:**
```
Sum of all task utilizations = Σ(exec_time / period)
```

For current system:
- SensorProcessing: 0.016%
- CommandLogic: 0.020%
- FaultMonitoring: 0.013%
- NavigationControl: 0.018%
- DataCollector: 0.013%
- LocalInterface: 0.006%
- **Total: ~0.086%** (excellent headroom)

### Deadline Violations

**Zero violations**: System meeting all real-time constraints
**Occasional violations**: Investigate with stress testing
**Frequent violations**: Critical - adjust periods or optimize algorithms

## Integration with Existing Systems

### Watchdog Monitoring

Performance monitoring complements the watchdog system:

- **Watchdog**: Detects task hangs (no heartbeat)
- **Performance Monitor**: Detects slow execution (deadline misses)

Both systems log to structured logging for correlation.

### Structured Logging

Performance events integrate with the existing logging system:

```cpp
LOG_WARN(MAIN) << "task" << task_name << "exec_us" << execution_us
               << "deadline_us" << deadline_us << "overrun_us" << overrun
               << "event" << "deadline_miss";
```

All logs can be filtered and analyzed:
```bash
grep "deadline_miss" logs/*.log
grep "high_utilization" logs/*.log
```

## Overhead Analysis

**Measurement Overhead:**
- 2× `std::chrono::steady_clock::now()` calls per iteration
- Typical overhead: < 1μs per measurement
- Negligible compared to task execution times (10-200μs)

**Memory Overhead:**
- ~500 bytes per registered task (TaskStats structure)
- Sliding window: 100 samples × 8 bytes = 800 bytes per task
- Total: ~1.3KB per task (negligible)

**Thread Safety:**
- Single mutex per PerformanceMonitor instance
- Lock held only during registration and measurement recording
- No contention observed (tasks execute sequentially)

## Advanced Features

### 1. Runtime Statistics Access

Query performance data from code:

```cpp
auto stats = perf_monitor.get_stats("SensorProcessing");
std::cout << "Max execution: " << stats.max_execution_us << "μs\n";

if (perf_monitor.has_deadline_violations()) {
    LOG_WARN(MAIN) << "event" << "system_overloaded";
}
```

### 2. Reset Statistics

Clear accumulated statistics:

```cpp
perf_monitor.reset_stats("TaskName");  // Reset one task
perf_monitor.reset_all_stats();        // Reset all tasks
```

### 3. Programmatic Reports

Generate formatted reports:

```cpp
std::string report = perf_monitor.get_report_string();
// Save to file, send via network, etc.
```

## Future Enhancements

Potential improvements for future stages:

1. **Real-time Graphing**: Live performance dashboard
2. **Historical Analysis**: Export statistics to CSV
3. **Predictive Alerts**: Machine learning for anomaly detection
4. **Per-iteration Metrics**: Track individual loop iterations
5. **Thread Priority Analysis**: Correlate with scheduler behavior
6. **Cache Performance**: L1/L2 cache miss tracking
7. **Context Switch Monitoring**: Track involuntary preemptions

## Testing Scenarios

### Normal Operation

Run system for 30 seconds:
```bash
./build/truck_control
# Wait 30 seconds, press Ctrl+C
# Observe: All utilization < 1%, zero violations
```

### Stress Testing

Increase load to detect limits:
1. Reduce task periods (e.g., 50ms → 10ms)
2. Add computational load (more filtering)
3. Increase number of trucks (future)
4. Inject faults and complex scenarios

### Deadline Violation Simulation

Intentionally cause violations:
```cpp
// In task_loop, add artificial delay
std::this_thread::sleep_for(std::chrono::milliseconds(200)); // > period
```

Observe:
- Log warnings for deadline misses
- Report shows violations count
- Utilization > 100%

## Conclusion

The Performance Monitoring System provides essential visibility into real-time task execution, enabling:

- **Validation** of timing constraints
- **Detection** of performance regressions
- **Optimization** guidance for algorithms
- **Debugging** of timing-related issues
- **Documentation** of system behavior

All with minimal overhead and zero algorithmic changes to existing code.

---

**Implementation Status**: ✅ Complete
**Files Modified**: 12 (6 headers, 6 implementations)
**Build Status**: ✅ Passing
**Test Status**: ✅ Verified with 5-second runtime

**Real-Time Automation Concepts Demonstrated**:
- WCET analysis ✓
- Deadline monitoring ✓
- Jitter measurement ✓
- CPU utilization calculation ✓
- Statistical performance analysis ✓
