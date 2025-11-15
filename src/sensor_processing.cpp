#include "sensor_processing.h"
#include "logger.h"
#include "watchdog.h"
#include <pthread.h>
#include <cstring>

SensorProcessing::SensorProcessing(CircularBuffer& buffer, size_t filter_order, int period_ms)
    : buffer_(buffer),
      filter_order_(filter_order),
      period_ms_(period_ms),
      running_(false),
      current_raw_data_{0, 0, 0, 20, false, false} {
}

SensorProcessing::~SensorProcessing() {
    stop();
}

void SensorProcessing::start() {
    if (running_) {
        return;
    }

    running_ = true;
    task_thread_ = std::thread(&SensorProcessing::task_loop, this);
    pthread_t native_handle = task_thread_.native_handle();
    struct sched_param param;
    param.sched_priority = 60;

    int result = pthread_setschedparam(native_handle, SCHED_FIFO, &param);
    if (result == 0) {
        LOG_INFO(SP) << "event" << "start" << "period_ms" << period_ms_ << "filter_order" << filter_order_ << "rt_priority" << 60 << "sched" << "FIFO";
    } else {
        LOG_WARN(SP) << "event" << "start" << "rt_priority" << "failed" << "errno" << result;
    }
}

void SensorProcessing::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (task_thread_.joinable()) {
        task_thread_.join();
    }

    LOG_INFO(SP) << "event" << "stop";
}

void SensorProcessing::set_raw_data(const RawSensorData& data) {
    std::lock_guard<std::mutex> lock(raw_data_mutex_);
    current_raw_data_ = data;
}

void SensorProcessing::task_loop() {
    auto next_execution = std::chrono::steady_clock::now();

    while (running_) {

        RawSensorData raw_data;
        {
            std::lock_guard<std::mutex> lock(raw_data_mutex_);
            raw_data = current_raw_data_;
        }


        int filtered_x = apply_moving_average(raw_data.position_x, position_x_history_);
        int filtered_y = apply_moving_average(raw_data.position_y, position_y_history_);
        int filtered_angle = apply_moving_average(raw_data.angle_x, angle_x_history_);
        int filtered_temp = apply_moving_average(raw_data.temperature, temperature_history_);


        SensorData processed_data;
        processed_data.position_x = filtered_x;
        processed_data.position_y = filtered_y;
        processed_data.angle_x = filtered_angle;
        processed_data.temperature = filtered_temp;
        processed_data.fault_electrical = raw_data.fault_electrical;
        processed_data.fault_hydraulic = raw_data.fault_hydraulic;


        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        processed_data.timestamp = ms.count();


        buffer_.write(processed_data);


        static int write_count = 0;
        if (++write_count % 50 == 0) {
            LOG_DEBUG(SP) << "event" << "write"
                          << "temp" << processed_data.temperature
                          << "pos_x" << processed_data.position_x
                          << "pos_y" << processed_data.position_y;
        }

        if (Watchdog::get_instance()) {
            Watchdog::get_instance()->heartbeat("SensorProcessing");
        }

        next_execution += std::chrono::milliseconds(period_ms_);
        std::this_thread::sleep_until(next_execution);
    }
}

int SensorProcessing::apply_moving_average(int new_value, std::deque<int>& history) {
    history.push_back(new_value);
    if (history.size() > filter_order_) {
        history.pop_front();
    }

    int sum = std::accumulate(history.begin(), history.end(), 0);
    int average = sum / static_cast<int>(history.size());

    return average;
}
