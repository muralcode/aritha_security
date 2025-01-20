
// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#pragma once

extern "C" {
#include <libavutil/frame.h>
}

#include <thread>
#include <atomic>
#include "logger.h"
#include "buffer_queue.h"
#include "video_capture.h"

// A basic motion detection module: reads from inQueue, processes frames,
// and pushes them to outQueue (for encoding).
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

private:
    BufferQueue<DecodedFrame, 128>& m_inQueue;
    BufferQueue<DecodedFrame, 128>& m_outQueue;

    double m_threshold;
    int    m_frameInterval;

    AVFrame* m_prevFrame;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};
