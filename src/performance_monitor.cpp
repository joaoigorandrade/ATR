#include "performance_monitor.h"
#include "logger.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

void PerformanceMonitor::register_task(const std::string& task_name, int expected_period_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    TaskStats stats;
    stats.task_name = task_name;
    stats.expected_period_ms = expected_period_ms;

    task_stats_[task_name] = stats;

    LOG_INFO(MAIN) << "task" << task_name << "period_ms" << expected_period_ms
                   << "event" << "perf_registered";
}

std::chrono::steady_clock::time_point PerformanceMonitor::start_measurement(
    const std::string& task_name) {
    // No lock needed - just return current time
    return std::chrono::steady_clock::now();
}

void PerformanceMonitor::end_measurement(const std::string& task_name,
                                        std::chrono::steady_clock::time_point start_time) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    long execution_us = duration.count();

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = task_stats_.find(task_name);
    if (it == task_stats_.end()) {
        // Task not registered - auto-register with 0 period
        LOG_WARN(MAIN) << "task" << task_name << "event" << "auto_register_perf";
        task_stats_[task_name] = TaskStats();
        it = task_stats_.find(task_name);
    }

    update_statistics(it->second, execution_us);

    // Check for deadline violation
    long deadline_us = it->second.expected_period_ms * 1000;
    if (deadline_us > 0 && execution_us > deadline_us) {
        it->second.deadline_violations++;
        long overrun = execution_us - deadline_us;

        if (overrun > it->second.worst_overrun_us) {
            it->second.worst_overrun_us = overrun;
        }

        LOG_WARN(MAIN) << "task" << task_name << "exec_us" << execution_us
                       << "deadline_us" << deadline_us << "overrun_us" << overrun
                       << "event" << "deadline_miss";
    }

    // Log if execution time is unusually high (> 80% of period)
    if (deadline_us > 0 && execution_us > (deadline_us * 0.8)) {
        LOG_WARN(MAIN) << "task" << task_name << "exec_us" << execution_us
                       << "deadline_us" << deadline_us
                       << "utilization_pct" << (100.0 * execution_us / deadline_us)
                       << "event" << "high_utilization";
    }
}

void PerformanceMonitor::update_statistics(TaskStats& stats, long execution_us) {
    stats.current_execution_us = execution_us;
    stats.sample_count++;

    // Update min/max
    if (execution_us < stats.min_execution_us) {
        stats.min_execution_us = execution_us;
    }
    if (execution_us > stats.max_execution_us) {
        stats.max_execution_us = execution_us;
    }

    // Add to recent samples (sliding window)
    stats.recent_samples.push_back(execution_us);
    if (stats.recent_samples.size() > 100) {
        stats.recent_samples.erase(stats.recent_samples.begin());
    }

    // Calculate running average (incremental formula)
    double delta = execution_us - stats.avg_execution_us;
    stats.avg_execution_us += delta / stats.sample_count;

    // Calculate standard deviation from recent samples
    if (stats.recent_samples.size() >= 2) {
        stats.std_dev_us = calculate_std_dev(stats.recent_samples, stats.avg_execution_us);
    }
}

double PerformanceMonitor::calculate_std_dev(const std::vector<long>& samples, double mean) const {
    if (samples.empty()) return 0.0;

    double sum_sq_diff = 0.0;
    for (long sample : samples) {
        double diff = sample - mean;
        sum_sq_diff += diff * diff;
    }

    return std::sqrt(sum_sq_diff / samples.size());
}

PerformanceMonitor::TaskStats PerformanceMonitor::get_stats(const std::string& task_name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = task_stats_.find(task_name);
    if (it != task_stats_.end()) {
        return it->second;
    }

    return TaskStats(); // Return empty stats if not found
}

std::map<std::string, PerformanceMonitor::TaskStats> PerformanceMonitor::get_all_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return task_stats_;
}

void PerformanceMonitor::reset_stats(const std::string& task_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = task_stats_.find(task_name);
    if (it != task_stats_.end()) {
        int period = it->second.expected_period_ms;
        std::string name = it->second.task_name;

        it->second = TaskStats();
        it->second.task_name = name;
        it->second.expected_period_ms = period;

        LOG_INFO(MAIN) << "task" << task_name << "event" << "perf_reset";
    }
}

void PerformanceMonitor::reset_all_stats() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& pair : task_stats_) {
        int period = pair.second.expected_period_ms;
        std::string name = pair.second.task_name;

        pair.second = TaskStats();
        pair.second.task_name = name;
        pair.second.expected_period_ms = period;
    }

    LOG_INFO(MAIN) << "event" << "perf_reset_all";
}

void PerformanceMonitor::print_report() const {
    std::cout << get_report_string() << std::endl;
}

std::string PerformanceMonitor::get_report_string() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;

    oss << "\n========================================\n";
    oss << "    TASK PERFORMANCE REPORT\n";
    oss << "========================================\n\n";

    if (task_stats_.empty()) {
        oss << "No performance data available.\n";
        return oss.str();
    }

    // Header
    oss << std::left
        << std::setw(20) << "Task"
        << std::setw(10) << "Period"
        << std::setw(12) << "Current"
        << std::setw(12) << "Min"
        << std::setw(12) << "Avg"
        << std::setw(12) << "Max"
        << std::setw(12) << "Std Dev"
        << std::setw(10) << "Util%"
        << std::setw(10) << "Violations"
        << "\n";

    oss << std::string(110, '-') << "\n";

    // Data rows
    for (const auto& pair : task_stats_) {
        const TaskStats& stats = pair.second;

        double utilization_pct = 0.0;
        if (stats.expected_period_ms > 0) {
            utilization_pct = 100.0 * stats.avg_execution_us / (stats.expected_period_ms * 1000.0);
        }

        oss << std::left
            << std::setw(20) << stats.task_name
            << std::setw(10) << (std::to_string(stats.expected_period_ms) + "ms")
            << std::setw(12) << (std::to_string(stats.current_execution_us) + "μs")
            << std::setw(12) << (stats.min_execution_us == LONG_MAX ? "-" : std::to_string(stats.min_execution_us) + "μs")
            << std::setw(12) << (std::to_string(static_cast<long>(stats.avg_execution_us)) + "μs")
            << std::setw(12) << (std::to_string(stats.max_execution_us) + "μs")
            << std::setw(12) << (std::to_string(static_cast<long>(stats.std_dev_us)) + "μs")
            << std::setw(10) << std::fixed << std::setprecision(1) << utilization_pct
            << std::setw(10) << stats.deadline_violations
            << "\n";
    }

    oss << std::string(110, '-') << "\n";

    // Summary
    int total_violations = 0;
    for (const auto& pair : task_stats_) {
        total_violations += pair.second.deadline_violations;
    }

    oss << "\nSummary:\n";
    oss << "  Total Tasks: " << task_stats_.size() << "\n";
    oss << "  Total Deadline Violations: " << total_violations << "\n";

    if (total_violations > 0) {
        oss << "  ⚠ WARNING: Deadline violations detected!\n";
    } else {
        oss << "  ✓ All tasks meeting deadlines\n";
    }

    oss << "========================================\n";

    return oss.str();
}

bool PerformanceMonitor::has_deadline_violations() const {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& pair : task_stats_) {
        if (pair.second.deadline_violations > 0) {
            return true;
        }
    }

    return false;
}
