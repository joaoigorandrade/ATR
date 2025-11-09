#include "circular_buffer.h"
#include <cstring>

CircularBuffer::CircularBuffer()
    : read_index_(0), write_index_(0), count_(0) {
    // Initialize buffer with zeros
    std::memset(buffer_, 0, sizeof(buffer_));
}

CircularBuffer::~CircularBuffer() {
    // Condition variables and mutex are automatically destroyed
}

void CircularBuffer::write(const SensorData& data) {
    // Lock mutex for critical section
    std::unique_lock<std::mutex> lock(mutex_);

    // Wait while buffer is full
    // Condition variable automatically releases mutex and waits
    // When notified, it re-acquires mutex before proceeding
    not_full_.wait(lock, [this]() { return count_ < BUFFER_SIZE; });

    // Critical section: write data to buffer
    buffer_[write_index_] = data;

    // Advance write index in circular manner
    write_index_ = (write_index_ + 1) % BUFFER_SIZE;

    // Increment count of elements in buffer
    count_++;

    // Mutex is automatically released when lock goes out of scope
    lock.unlock();

    // Notify one waiting consumer that data is available
    not_empty_.notify_one();
}

SensorData CircularBuffer::read() {
    // Lock mutex for critical section
    std::unique_lock<std::mutex> lock(mutex_);

    // Wait while buffer is empty
    not_empty_.wait(lock, [this]() { return count_ > 0; });

    // Critical section: read data from buffer
    SensorData data = buffer_[read_index_];

    // Advance read index in circular manner
    read_index_ = (read_index_ + 1) % BUFFER_SIZE;

    // Decrement count of elements in buffer
    count_--;

    // Release lock
    lock.unlock();

    // Notify one waiting producer that space is available
    not_full_.notify_one();

    return data;
}

SensorData CircularBuffer::peek_latest() {
    // Non-blocking read of most recent data
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ == 0) {
        // Return zeroed data if buffer is empty
        SensorData empty_data = {};
        return empty_data;
    }

    // Calculate the index of the most recently written data
    // write_index_ points to next write position, so subtract 1 (with wrap)
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
