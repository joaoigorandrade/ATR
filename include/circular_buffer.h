#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <mutex>
#include <condition_variable>
#include <cstddef>

/**
 * @brief Sensor data structure stored in the circular buffer
 *
 * Contains processed sensor readings after noise filtering.
 * This represents the shared data model accessed by multiple tasks.
 */
struct SensorData {
    int position_x;      // Vehicle position X coordinate (absolute reference)
    int position_y;      // Vehicle position Y coordinate (absolute reference)
    int angle_x;         // Vehicle angular direction (degrees, 0 = East)
    int temperature;     // Engine temperature (°C, range: -100 to +200)
    bool fault_electrical;  // Electrical system fault flag
    bool fault_hydraulic;   // Hydraulic system fault flag
    long timestamp;      // Timestamp in milliseconds
};

/**
 * @brief Thread-safe circular buffer for producer-consumer pattern
 *
 * Implements a fixed-size circular buffer with semaphore-based synchronization.
 * Multiple producers (Sensor Processing) can write, and multiple consumers
 * (Command Logic, Navigation Control, etc.) can read concurrently.
 *
 * Synchronization mechanisms:
 * - Mutex: Protects critical sections during read/write operations
 * - Condition Variables: Signal when buffer state changes (empty/full)
 *
 * This follows the classic Producer-Consumer synchronization pattern
 * covered in real-time automation (bounded buffer problem).
 * Using condition variables instead of semaphores for better portability.
 */
class CircularBuffer {
public:
    static constexpr size_t BUFFER_SIZE = 200;  // As specified in requirements

    /**
     * @brief Construct circular buffer and initialize synchronization primitives
     */
    CircularBuffer();

    /**
     * @brief Destroy circular buffer and clean up semaphores
     */
    ~CircularBuffer();

    /**
     * @brief Write sensor data to buffer (producer operation)
     *
     * Blocks if buffer is full until space becomes available.
     * Uses condition variable wait on not_full condition.
     *
     * @param data Sensor data to write
     */
    void write(const SensorData& data);

    /**
     * @brief Read sensor data from buffer (consumer operation)
     *
     * Blocks if buffer is empty until data becomes available.
     * Uses condition variable wait on not_empty condition.
     *
     * @return SensorData The oldest data in the buffer (FIFO)
     */
    SensorData read();

    /**
     * @brief Peek at the most recent data without consuming it
     *
     * Non-blocking read of the last written value.
     * Useful for monitoring tasks that need current state.
     *
     * @return SensorData The most recent data
     */
    SensorData peek_latest();

    /**
     * @brief Get current buffer occupancy
     *
     * @return size_t Number of elements currently in buffer
     */
    size_t size() const;

    /**
     * @brief Check if buffer is empty
     *
     * @return true if buffer is empty
     */
    bool is_empty() const;

    /**
     * @brief Check if buffer is full
     *
     * @return true if buffer is full
     */
    bool is_full() const;

private:
    SensorData buffer_[BUFFER_SIZE];  // Fixed-size array for buffer storage
    size_t read_index_;               // Consumer read position
    size_t write_index_;              // Producer write position
    size_t count_;                    // Current number of elements

    mutable std::mutex mutex_;        // Mutex for critical section protection
    std::condition_variable not_full_;  // Condition: buffer is not full
    std::condition_variable not_empty_; // Condition: buffer is not empty
};

#endif // CIRCULAR_BUFFER_H
