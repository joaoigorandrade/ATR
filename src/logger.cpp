#include "logger.h"
#include <cstdlib>

namespace Logger {
static Level g_min_level = Level::INFO;
static std::mutex g_log_mutex;

void init(Level min_level) {
    const char* env_level = std::getenv("LOG_LEVEL");
    if (env_level) {
        std::string level_str(env_level);
        if (level_str == "DEBUG") g_min_level = Level::DEBUG;
        else if (level_str == "INFO") g_min_level = Level::INFO;
        else if (level_str == "WARN") g_min_level = Level::WARN;
        else if (level_str == "ERR") g_min_level = Level::ERR;
        else if (level_str == "CRIT") g_min_level = Level::CRIT;
        else g_min_level = min_level;
    } else {
        g_min_level = min_level;
    }
}

void set_level(Level min_level) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    g_min_level = min_level;
}

Level get_level() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    return g_min_level;
}

const char* level_str(Level level) {
    switch (level) {
        case Level::DEBUG: return "DBG";
        case Level::INFO:  return "INF";
        case Level::WARN:  return "WRN";
        case Level::ERR:   return "ERR";
        case Level::CRIT:  return "CRT";
        default:           return "???";
    }
}

const char* module_str(Module module) {
    switch (module) {
        case Module::MAIN: return "MA";
        case Module::SP:   return "SP";
        case Module::CB:   return "CB";
        case Module::CL:   return "CL";
        case Module::FM:   return "FM";
        case Module::NC:   return "NC";
        case Module::RP:   return "RP";
        case Module::DC:   return "DC";
        case Module::LI:   return "LI";
        default:           return "??";
    }
}

long timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return ms.count();
}

LogStream::LogStream(Level level, Module module)
    : level_(level),
      module_(module),
      should_log_(level >= get_level()),
      expect_key_(true),
      first_pair_(true) { }

LogStream::~LogStream() {
    if (!should_log_) return;

    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::cout << timestamp_ms() << "|"
              << level_str(level_) << "|"
              << module_str(module_) << "|"
              << stream_.str()
              << std::endl;
}

LogStream log(Level level, Module module) {
    return LogStream(level, module);
}
}
