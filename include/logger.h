#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iostream>

/**
 * @file logger.h
 * @brief AI-Optimized Structured Logging System
 *
 * Design Goals:
 * - Minimal token usage for AI agents
 * - Structured key=value format for easy parsing
 * - Compact module identifiers
 * - Runtime log level filtering
 * - Thread-safe operations
 *
 * Format: timestamp|level|module|event|data
 * Example: 1731283456789|INFO|SP|WRITE|temp=75,buf=42
 */

namespace Logger {

// Log levels (ascending severity)
enum class Level {
    DEBUG = 0,  // Detailed diagnostic information
    INFO  = 1,  // General informational messages
    WARN  = 2,  // Warning conditions
    ERR   = 3,  // Error conditions
    CRIT  = 4   // Critical failures
};

// Module identifiers (2-3 char codes for compactness)
enum class Module {
    MAIN,   // Main application
    SP,     // Sensor Processing
    CB,     // Circular Buffer
    CL,     // Command Logic
    FM,     // Fault Monitoring
    NC,     // Navigation Control
    RP,     // Route Planning
    DC,     // Data Collector
    LI      // Local Interface
};

/**
 * @class LogStream
 * @brief Builder pattern for constructing log messages
 *
 * Usage:
 *   log(Level::INFO, Module::SP) << "temp" << 75 << "status" << "ok";
 * Output:
 *   1731283456789|INFO|SP|temp=75,status=ok
 */
class LogStream {
public:
    LogStream(Level level, Module module);
    ~LogStream();

    // Stream operator for key-value pairs
    template<typename T>
    LogStream& operator<<(const T& value) {
        if (!should_log_) return *this;

        // Toggle between key and value
        if (expect_key_) {
            // This is a key - store it
            buffer_ << value;
            pending_key_ = buffer_.str();
            buffer_.str("");
            buffer_.clear();
            expect_key_ = false;
        } else {
            // This is a value - output key=value pair
            if (!first_pair_) {
                stream_ << ",";
            }
            stream_ << pending_key_ << "=" << value;
            first_pair_ = false;
            expect_key_ = true;
        }
        return *this;
    }

private:
    Level level_;
    Module module_;
    bool should_log_;
    bool expect_key_;
    bool first_pair_;
    std::string pending_key_;
    std::ostringstream stream_;
    std::ostringstream buffer_;
};

/**
 * @brief Initialize logger with minimum log level
 * @param min_level Minimum level to log (default: INFO)
 */
void init(Level min_level = Level::INFO);

/**
 * @brief Set minimum log level at runtime
 */
void set_level(Level min_level);

/**
 * @brief Get current minimum log level
 */
Level get_level();

/**
 * @brief Create a log entry
 * @param level Log severity level
 * @param module Module identifier
 * @return LogStream for building message
 */
LogStream log(Level level, Module module);

/**
 * @brief Convert level enum to string
 */
const char* level_str(Level level);

/**
 * @brief Convert module enum to string
 */
const char* module_str(Module module);

/**
 * @brief Get current timestamp in milliseconds
 */
long timestamp_ms();

} // namespace Logger

// Convenience macros for common log patterns
#define LOG_DEBUG(mod) Logger::log(Logger::Level::DEBUG, Logger::Module::mod)
#define LOG_INFO(mod)  Logger::log(Logger::Level::INFO,  Logger::Module::mod)
#define LOG_WARN(mod)  Logger::log(Logger::Level::WARN,  Logger::Module::mod)
#define LOG_ERR(mod)   Logger::log(Logger::Level::ERR,   Logger::Module::mod)
#define LOG_CRIT(mod)  Logger::log(Logger::Level::CRIT,  Logger::Module::mod)

#endif // LOGGER_H
