// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

// Encodes raw frames using configurable codec settings for H.264, H.265, and AV1.
// Parameters like bitrate, GOP size, and resolution were abstracted into reusable CodecConfig structures,
// allowing easy tuning for different environments.

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <thread>
#include <atomic>
#include <string>
#include "logger.h"
#include "buffer_queue.h"
#include "video_capture.h" // for DecodedFrame

// Packets produced by encoder
struct EncodedPacket {
    AVPacket* packet = nullptr;
};

class VideoEncoder {
public:
    VideoEncoder(BufferQueue<DecodedFrame, 128>& inQueue,
                 BufferQueue<EncodedPacket, 128>& outQueue,
                 int width,
                 int height,
                 int fps,
                 const std::string& codecName,
                 bool hwAccel);
    ~VideoEncoder();

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    void encodingLoop();
    bool initEncoder();
    void closeEncoder();

private:
    BufferQueue<DecodedFrame, 128>& m_inQueue;
    BufferQueue<EncodedPacket, 128>& m_outQueue;

    int m_width;
    int m_height;
    int m_fps;
    std::string m_codecName;
    bool m_hwAccel;

    AVCodecContext* m_codecCtx = nullptr;
    SwsContext*     m_swsCtx   = nullptr;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    bool m_initialized{false};
};
