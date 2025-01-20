// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>

enum class LogLevel {
    ERROR,
    WARNING,
    INFO,
    DEBUG
};

class Logger {
public:
    static Logger& instance();

    // Initialize logger with a file path, optional console output, and verbosity
    void init(const std::string& filePath, bool consoleOutput, bool verbose);

    // Log a message with a certain level
    void log(LogLevel level, const std::string& msg);

private:
    Logger() = default;
    ~Logger();

    std::ofstream m_logFile;
    std::mutex m_mutex;
    bool m_consoleOutput{false};
    bool m_verbose{false};

    std::string levelToString(LogLevel level);
};

// Helper macros
#define LOG_ERROR(msg)   Logger::instance().log(LogLevel::ERROR, (msg))
#define LOG_WARNING(msg) Logger::instance().log(LogLevel::WARNING, (msg))
#define LOG_INFO(msg)    Logger::instance().log(LogLevel::INFO, (msg))
#define LOG_DEBUG(msg)   Logger::instance().log(LogLevel::DEBUG, (msg))
