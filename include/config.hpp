// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#pragma once

#include <string>
#include <memory>

struct Config {
    // Input stream (e.g., RTSP URL)
    std::string inputUrl;

    // Output (e.g., RTMP URL or local file path)
    std::string outputUrl;

    // Video encoding parameters
    int width;
    int height;
    int fps;
    std::string codecName; // e.g., "libx264", "h264_nvenc", etc.

    // Motion detection parameters
    double motionThreshold;
    int motionFrameInterval;

    // Logging
    std::string logFilePath;
    bool verboseLogs;

    // Other
    bool enableHardwareAccel;
    bool reconnectOnFailure;
    int reconnectDelaySecs;

    void validate() const;
};

std::shared_ptr<Config> loadConfig(const std::string& filename);
