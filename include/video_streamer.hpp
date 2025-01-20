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
#include <buffer_queue.hpp>
#include <logger.hpp>
#include <video_encoder.hpp> // this is for EncodedPacket

class VideoStreamer {
public:
    VideoStreamer(BufferQueue<EncodedPacket, 128>& inQueue, 
                  const std::string& outputUrl);
    ~VideoStreamer();

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    void streamingLoop();
    bool initOutput();
    void closeOutput();

    BufferQueue<EncodedPacket, 128>& m_inQueue;
    std::string m_outputUrl;

    AVFormatContext* m_fmtCtx = nullptr;
    AVStream* m_videoStream = nullptr;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    bool m_initialized{false};
};
