// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#include <config.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

void Config::validate() const {
    if (inputUrl.empty()) {
        throw std::runtime_error("Config error: inputUrl is empty.");
    }
    if (outputUrl.empty()) {
        throw std::runtime_error("Config error: outputUrl is empty.");
    }
    if (width <= 0 || height <= 0 || fps <= 0) {
        throw std::runtime_error("Config error: Invalid width/height/fps.");
    }
    if (codecName.empty()) {
        throw std::runtime_error("Config error: codecName is empty.");
    }
    if (motionThreshold < 0) {
        throw std::runtime_error("Config error: motionThreshold cannot be negative.");
    }
    if (motionFrameInterval <= 0) {
        throw std::runtime_error("Config error: motionFrameInterval must be > 0.");
    }
}

std::shared_ptr<Config> loadConfig(const std::string& filename) {
    auto cfg = std::make_shared<Config>();

    // Default values
    cfg->width  = 1280;
    cfg->height = 720;
    cfg->fps    = 30;
    cfg->codecName = "libx264";
    cfg->motionThreshold = 5.0;
    cfg->motionFrameInterval = 1;
    cfg->logFilePath = "surveillance.log";
    cfg->verboseLogs = false;
    cfg->enableHardwareAccel = false;
    cfg->reconnectOnFailure  = true;
    cfg->reconnectDelaySecs  = 5;

    if (filename.empty()) {
        // If no config file was provided, use defaults
        cfg->inputUrl  = "rtsp://127.0.0.1/live/stream";
        cfg->outputUrl = "rtmp://127.0.0.1/live/out";
        cfg->validate();
        return cfg;
    }

    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "Could not open config file: " << filename << ". Using defaults.\n";
        cfg->inputUrl  = "rtsp://127.0.0.1/live/stream";
        cfg->outputUrl = "rtmp://127.0.0.1/live/out";
        cfg->validate();
        return cfg;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        if (key == "inputUrl") {
            iss >> cfg->inputUrl;
        } else if (key == "outputUrl") {
            iss >> cfg->outputUrl;
        } else if (key == "width") {
            iss >> cfg->width;
        } else if (key == "height") {
            iss >> cfg->height;
        } else if (key == "fps") {
            iss >> cfg->fps;
        } else if (key == "codecName") {
            iss >> cfg->codecName;
        } else if (key == "motionThreshold") {
            iss >> cfg->motionThreshold;
        } else if (key == "motionFrameInterval") {
            iss >> cfg->motionFrameInterval;
        } else if (key == "logFilePath") {
            iss >> cfg->logFilePath;
        } else if (key == "verboseLogs") {
            int tmp;
            iss >> tmp;
            cfg->verboseLogs = (tmp != 0);
        } else if (key == "enableHardwareAccel") {
            int tmp;
            iss >> tmp;
            cfg->enableHardwareAccel = (tmp != 0);
        } else if (key == "reconnectOnFailure") {
            int tmp;
            iss >> tmp;
            cfg->reconnectOnFailure = (tmp != 0);
        } else if (key == "reconnectDelaySecs") {
            iss >> cfg->reconnectDelaySecs;
        }
    }
    in.close();
    cfg->validate();
    return cfg;
}
