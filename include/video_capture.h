// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <string>
#include <thread>
#include <atomic>
#include "logger.h"
#include "buffer_queue.h"

// A container to pass decoded frames
struct DecodedFrame {
    AVFrame* frame = nullptr;
    int64_t pts    = 0;
};

class VideoCapture {
public:
    VideoCapture(const std::string& inputUrl,
                 BufferQueue<DecodedFrame, 128>& captureQueue,
                 bool reconnectOnFailure,
                 int reconnectDelaySecs);
    ~VideoCapture();

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    void captureLoop();
    bool openStream();
    void closeStream();

private:
    std::string m_inputUrl;
    bool m_reconnectOnFailure;
    int m_reconnectDelaySecs;

    BufferQueue<DecodedFrame, 128>& m_captureQueue;

    AVFormatContext* m_fmtCtx = nullptr;
    AVCodecContext*  m_codecCtx = nullptr;
    int m_videoStreamIndex = -1;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
};
