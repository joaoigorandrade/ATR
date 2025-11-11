#include "circular_buffer.h"
#include "logger.h"
#include <cstring>

CircularBuffer::CircularBuffer()
    : read_index_(0), write_index_(0), count_(0) {
    std::memset(buffer_, 0, sizeof(buffer_));
}

CircularBuffer::~CircularBuffer() { }

void CircularBuffer::write(const SensorData& data) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (count_ == BUFFER_SIZE) {
        read_index_ = (read_index_ + 1) % BUFFER_SIZE;
        count_--;

        static int overwrite_count = 0;
        if (++overwrite_count % 100 == 0) {
            LOG_WARN(CB) << "event" << "overwrite" << "count" << overwrite_count;
        }
    }

    buffer_[write_index_] = data;
    write_index_ = (write_index_ + 1) % BUFFER_SIZE;
    count_++;

    lock.unlock();

    not_empty_.notify_one();
}

SensorData CircularBuffer::read() {
    std::unique_lock<std::mutex> lock(mutex_);

    not_empty_.wait(lock, [this]() { return count_ > 0; });

    SensorData data = buffer_[read_index_];
    read_index_ = (read_index_ + 1) % BUFFER_SIZE;
    count_--;


    lock.unlock();
    not_full_.notify_one();
    return data;
}

SensorData CircularBuffer::peek_latest() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ == 0) {
        SensorData empty_data = {};
        return empty_data;
    }
    size_t latest_index = (write_index_ == 0) ? (BUFFER_SIZE - 1) : (write_index_ - 1);

    return buffer_[latest_index];
}

size_t CircularBuffer::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

bool CircularBuffer::is_empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_ == 0;
}

bool CircularBuffer::is_full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_ == BUFFER_SIZE;
}
