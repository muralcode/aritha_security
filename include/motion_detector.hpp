// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

//TODO:  future intgrate AI into this 
//perhaps to detect cars ,people, pets etc.

#pragma once

extern "C" {
#include <libavutil/frame.h>
}

#include <thread>
#include <atomic>

#include <logger.hpp>

// A container for decoded frames
struct DecodedFrame {
    AVFrame* frame = nullptr;
    int64_t pts = 0;
};

class MotionDetector {
public:
    MotionDetector(BufferQueue<DecodedFrame, 128>& inQueue,
                   BufferQueue<DecodedFrame, 128>& outQueue,
                   double threshold,
                   int frameInterval);
    ~MotionDetector();

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    void detectionLoop();

    BufferQueue<DecodedFrame, 128>& m_inQueue;
    BufferQueue<DecodedFrame, 128>& m_outQueue;

    double m_threshold;
    int    m_frameInterval;

    AVFrame* m_prevFrame = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};
