// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#include <logger.hpp>
#include <iostream>
#include <ctime>

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
}

void Logger::init(const std::string& filePath, bool consoleOutput, bool verbose) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_logFile.is_open()) {
        m_logFile.close();
    }
    m_logFile.open(filePath, std::ios::app);
    m_consoleOutput = consoleOutput;
    m_verbose = verbose;
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::DEBUG:   return "DEBUG";
        default:                return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& msg) {
    // Skip DEBUG if not verbose
    if (!m_verbose && level == LogLevel::DEBUG) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Get timestamp
    std::time_t t = std::time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    std::ostringstream oss;
    oss << "[" << buf << "] [" << levelToString(level) << "] " << msg << "\n";

    if (m_logFile.is_open()) {
        m_logFile << oss.str();
        m_logFile.flush();
    }

    if (m_consoleOutput) {
        std::cout << oss.str();
    }
}
