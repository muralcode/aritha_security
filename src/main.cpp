// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#include <iostream>
#include <config.hpp>
#include <logger.hpp>
#include <buffer_queue.hpp>
#include <motion_detector.hpp>
#include <video_capture.hpp>
#include <video_encoder.hpp>
#include <video_streamer.hpp>
#include <utilities.hpp>

int main(int argc, char** argv) {
    // 1. Load config from file or default
    std::string configFile = (argc > 1) ? argv[1] : "";
    auto config = loadConfig(configFile);

    // 2. Initialize logger
    Logger::instance().init(config->logFilePath, /*consoleOutput=*/true, config->verboseLogs);
    LOG_INFO("Starting HomeSurveillance...");

    // 3. Create queues
    static BufferQueue<DecodedFrame, 128> captureQueue;
    static BufferQueue<DecodedFrame, 128> motionToEncoderQueue;
    static BufferQueue<EncodedPacket, 128> encoderToStreamerQueue;

    // 4. Create modules
    VideoCapture capture(config->inputUrl,
                         captureQueue,
                         config->reconnectOnFailure,
                         config->reconnectDelaySecs);

    // The Motion Detector pushes frames to motionToEncoderQueue
    MotionDetector motion(captureQueue,
                          motionToEncoderQueue,
                          config->motionThreshold,
                          config->motionFrameInterval);

    VideoEncoder encoder(motionToEncoderQueue,
                         encoderToStreamerQueue,
                         config->width,
                         config->height,
                         config->fps,
                         config->codecName,
                         config->enableHardwareAccel);

    VideoStreamer streamer(encoderToStreamerQueue,
                           config->outputUrl);

    // 5. Start the pipeline
    capture.start();
    motion.start();
    encoder.start();
    streamer.start();

    // 6. Let it run for 60 seconds in this demo
    LOG_INFO("System running... will stop in ~60 seconds...");
    for (int i = 0; i < 60; ++i) {
        // If any stage unexpectedly stops, we exit
        if (!capture.isRunning() || !motion.isRunning() ||
            !encoder.isRunning() || !streamer.isRunning()) {
            LOG_ERROR("A module has stopped unexpectedly. Exiting.");
            break;
        }
        sleepMs(1000);
    }

    // 7. Stop modules
    LOG_INFO("Stopping system...");
    capture.stop();
    motion.stop();
    encoder.stop();
    streamer.stop();

    LOG_INFO("Aritha Security terminated gracefully.");
    return 0;
}
