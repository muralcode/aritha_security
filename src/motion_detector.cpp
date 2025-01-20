// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#include <motion_detector.hpp>
#include <cmath>
#include <thread>
#include <chrono>

MotionDetector::MotionDetector(BufferQueue<DecodedFrame, 128>& inQueue,
                               BufferQueue<DecodedFrame, 128>& outQueue,
                               double threshold,
                               int frameInterval)
    : m_inQueue(inQueue)
    , m_outQueue(outQueue)
    , m_threshold(threshold)
    , m_frameInterval(frameInterval)
{
}

MotionDetector::~MotionDetector() {
    stop();
    if (m_prevFrame) {
        av_frame_free(&m_prevFrame);
    }
}

void MotionDetector::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&MotionDetector::detectionLoop, this);
}

void MotionDetector::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void MotionDetector::detectionLoop() {
    int frameCount = 0;
    while (m_running.load()) {
        auto maybeFrame = m_inQueue.pop();
        if (!maybeFrame.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        DecodedFrame df = std::move(maybeFrame.value());
        AVFrame* current = df.frame;
        if (!current) {
            continue;
        }
        frameCount++;

        // Only compute difference every m_frameInterval frames
        if (m_prevFrame && (frameCount % m_frameInterval == 0)) {
            if (current->width == m_prevFrame->width &&
                current->height == m_prevFrame->height) {
                int width = current->width;
                int height = current->height;
                int strideCur = current->linesize[0];
                int stridePrev = m_prevFrame->linesize[0];

                double sumDiff = 0.0;
                int totalPixels = width * height;

                uint8_t* currData = current->data[0];
                uint8_t* prevData = m_prevFrame->data[0];

                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        int idxCur = y * strideCur + x;
                        int idxPrev = y * stridePrev + x;
                        double diff = std::abs((double)currData[idxCur] - (double)prevData[idxPrev]);
                        sumDiff += diff;
                    }
                }
                double avgDiff = sumDiff / totalPixels;
                if (avgDiff > m_threshold) {
                    LOG_INFO("MotionDetector: Motion detected. avgDiff=" + std::to_string(avgDiff));
                } else {
                    LOG_DEBUG("MotionDetector: No significant motion. avgDiff=" + std::to_string(avgDiff));
                }
            }
        }

        // Update previous frame
        if (!m_prevFrame) {
            m_prevFrame = av_frame_alloc();
        }
        av_frame_ref(m_prevFrame, current);

        // Pass frame along to next stage
        while (!m_outQueue.push(std::move(df))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    m_running.store(false);
}
