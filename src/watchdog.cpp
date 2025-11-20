#include "watchdog.h"
#include "logger.h"
#include <algorithm>

static Watchdog* g_watchdog_instance = nullptr;

Watchdog::Watchdog(int check_period_ms)
    : check_period_ms_(check_period_ms),
      running_(false),
      fault_count_(0) {
    fault_handler_ = std::bind(&Watchdog::default_fault_handler, this,
                               std::placeholders::_1, std::placeholders::_2);
}

Watchdog::~Watchdog() {
    stop();
}

void Watchdog::start() {
    if (running_) {
        return;
    }

    running_ = true;
    watchdog_thread_ = std::thread(&Watchdog::watchdog_loop, this);

    LOG_INFO(MAIN) << "event" << "watchdog_start" << "check_period_ms" << check_period_ms_;
}

void Watchdog::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }

    LOG_INFO(MAIN) << "event" << "watchdog_stop" << "faults_detected" << fault_count_.load();
}

void Watchdog::register_task(const std::string& task_name, int timeout_ms) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    TaskInfo info;
    info.timeout_ms = timeout_ms;
    info.last_heartbeat = std::chrono::steady_clock::now();
    info.ever_reported = false;
    info.consecutive_failures = 0;

    monitored_tasks_[task_name] = info;

    LOG_INFO(MAIN) << "event" << "watchdog_register" << "task" << task_name << "timeout_ms" << timeout_ms;
}

void Watchdog::unregister_task(const std::string& task_name) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    monitored_tasks_.erase(task_name);

    LOG_INFO(MAIN) << "event" << "watchdog_unregister" << "task" << task_name;
}

void Watchdog::heartbeat(const std::string& task_name) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto it = monitored_tasks_.find(task_name);
    if (it != monitored_tasks_.end()) {
        it->second.last_heartbeat = std::chrono::steady_clock::now();
        it->second.ever_reported = true;
        it->second.consecutive_failures = 0;

        static int heartbeat_count = 0;
        if (++heartbeat_count % 100 == 0) {
            LOG_DEBUG(MAIN) << "event" << "watchdog_heartbeat" << "task" << task_name << "count" << heartbeat_count;
        }
    } else {
        LOG_WARN(MAIN) << "event" << "watchdog_heartbeat_unknown" << "task" << task_name;
    }
}

void Watchdog::set_fault_handler(FaultHandler handler) {
    fault_handler_ = handler;
}

size_t Watchdog::get_task_count() const {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    return monitored_tasks_.size();
}

void Watchdog::watchdog_loop() {
    auto next_check = std::chrono::steady_clock::now();

    while (running_) {
        {
            std::lock_guard<std::mutex> lock(tasks_mutex_);

            for (auto& [task_name, info] : monitored_tasks_) {
                if (is_timed_out(task_name, info)) {
                auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - info.last_heartbeat).count();

                    info.consecutive_failures++;
                    fault_count_++;

                    if (fault_handler_) {
                        fault_handler_(task_name, elapsed);
                    }

                    info.last_heartbeat = now;
                }
            }
        }

        next_check += std::chrono::milliseconds(check_period_ms_);
        std::this_thread::sleep_until(next_check);
    }
}

bool Watchdog::is_timed_out(const std::string& task_name, const TaskInfo& info) {
    if (!info.ever_reported) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - info.last_heartbeat).count();

    return elapsed > info.timeout_ms;
}

void Watchdog::default_fault_handler(const std::string& task_name, long elapsed_ms) {
    LOG_CRIT(MAIN) << "event" << "watchdog_fault"
                   << "task" << task_name
                   << "elapsed_ms" << elapsed_ms
                   << "total_faults" << fault_count_.load();
}

void Watchdog::set_instance(Watchdog* instance) {
    g_watchdog_instance = instance;
}

Watchdog* Watchdog::get_instance() {
    return g_watchdog_instance;
}
