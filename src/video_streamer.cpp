// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#include <video_streamer.hpp>
#include <chrono>
#include <thread>

VideoStreamer::VideoStreamer(BufferQueue<EncodedPacket, 128>& inQueue,
                             const std::string& outputUrl)
    : m_inQueue(inQueue)
    , m_outputUrl(outputUrl)
{
    avformat_network_init();
}

VideoStreamer::~VideoStreamer() {
    stop();
    closeOutput();
}

bool VideoStreamer::initOutput() {
    closeOutput();

    int ret = avformat_alloc_output_context2(&m_fmtCtx, nullptr, "flv", m_outputUrl.c_str());
    if (ret < 0 || !m_fmtCtx) {
        LOG_ERROR("Video Streamer: Failed to create output context for " + m_outputUrl);
        return false;
    }

    m_videoStream = avformat_new_stream(m_fmtCtx, nullptr);
    if (!m_videoStream) {
        LOG_ERROR("Video Streamer: Failed to create new stream.");
        return false;
    }

    // For real usage, copy encoder parameters to m_videoStream->codecpar
    // so the container has the correct info. Simplified here.

    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_fmtCtx->pb, m_outputUrl.c_str(), AVIO_FLAG_WRITE) < 0) {
            LOG_ERROR("VideoStreamer: Could not open output: " + m_outputUrl);
            return false;
        }
    }

    ret = avformat_write_header(m_fmtCtx, nullptr);
    if (ret < 0) {
        LOG_ERROR("VideoStreamer: Error writing header to " + m_outputUrl);
        return false;
    }

    m_initialized = true;
    LOG_INFO("VideoStreamer: Initialized output: " + m_outputUrl);
    return true;
}

void VideoStreamer::closeOutput() {
    if (m_fmtCtx) {
        av_write_trailer(m_fmtCtx);
        if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_fmtCtx->pb);
        }
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    m_videoStream = nullptr;
    m_initialized = false;
}

void VideoStreamer::start() {
    if (m_running.load()) return;
    if (!m_initialized) {
        if (!initOutput()) {
            LOG_ERROR("Video Streamer: Could not init output. Aborting start.");
            return;
        }
    }

    m_running.store(true);
    m_thread = std::thread(&VideoStreamer::streamingLoop, this);
}

void VideoStreamer::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void VideoStreamer::streamingLoop() {
    while (m_running.load()) {
        auto maybePkt = m_inQueue.pop();
        if (!maybePkt.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        EncodedPacket ep = std::move(maybePkt.value());
        AVPacket* pkt = ep.packet;
        if (!pkt) {
            continue;
        }

        // Typically you'd set pkt->stream_index = m_videoStream->index
        pkt->stream_index = m_videoStream->index;

        int ret = av_interleaved_write_frame(m_fmtCtx, pkt);
        if (ret < 0) {
            LOG_WARNING("Video Streamer: Error writing packet to " + m_outputUrl);
        }

        av_packet_free(&pkt);
    }
    m_running.store(false);
}
