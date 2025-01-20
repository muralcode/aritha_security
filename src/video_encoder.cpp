// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#include <video_encoder.hpp>
#include <thread>
#include <iostream>

VideoEncoder::VideoEncoder(BufferQueue<DecodedFrame, 128>& inQueue,
                           BufferQueue<EncodedPacket, 128>& outQueue,
                           int width,
                           int height,
                           int fps,
                           const std::string& codecName,
                           bool hwAccel)
    : m_inQueue(inQueue)
    , m_outQueue(outQueue)
    , m_width(width)
    , m_height(height)
    , m_fps(fps)
    , m_codecName(codecName)
    , m_hwAccel(hwAccel)
{
}

VideoEncoder::~VideoEncoder() {
    stop();
    closeEncoder();
}

bool VideoEncoder::initEncoder() {
    const AVCodec* codec = avcodec_find_encoder_by_name(m_codecName.c_str());
    if (!codec) {
        // fallback to H.264 if not found
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            LOG_ERROR("Video Encoder: Could not find " + m_codecName + " or fallback h264.");
            return false;
        }
        LOG_WARNING("Video Encoder: Using fallback h264 encoder instead of " + m_codecName);
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        LOG_ERROR("Video Encoder: Failed to allocate codec context.");
        return false;
    }

    m_codecCtx->width = m_width;
    m_codecCtx->height = m_height;
    m_codecCtx->time_base = (AVRational){1, m_fps};
    m_codecCtx->framerate = (AVRational){m_fps, 1};
    m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecCtx->thread_count = 4;
    m_codecCtx->gop_size = 12;
    m_codecCtx->max_b_frames = 2;

    // Some advanced settings if h264
    if (strstr(codec->name, "264") != nullptr) {
        av_opt_set(m_codecCtx->priv_data, "preset", "veryfast", 0);
        av_opt_set(m_codecCtx->priv_data, "tune", "zerolatency", 0);
    }

    if (m_hwAccel) {
        LOG_INFO("Video Encoder: Hardware acceleration requested (placeholder).");
        // Actual usage depends on NVENC, VAAPI, etc.
    }

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        LOG_ERROR("Video Encoder: Failed to open encoder.");
        return false;
    }

    m_initialized = true;
    LOG_INFO("Video Encoder: Encoder initialized: " + m_codecName);
    return true;
}

void VideoEncoder::closeEncoder() {
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    m_initialized = false;
}

void VideoEncoder::start() {
    if (m_running.load()) return;

    if (!m_initialized) {
        if (!initEncoder()) {
            LOG_ERROR("Video Encoder: Could not init encoder.");
            return;
        }
    }

    m_running.store(true);
    m_thread = std::thread(&VideoEncoder::encodingLoop, this);
}

void VideoEncoder::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }

    // Flush
    if (m_codecCtx) {
        avcodec_send_frame(m_codecCtx, nullptr);
        while (true) {
            AVPacket* pkt = av_packet_alloc();
            int ret = avcodec_receive_packet(m_codecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_free(&pkt);
                break;
            } else if (ret < 0) {
                LOG_ERROR("Video Encoder: Error flushing encoder.");
                av_packet_free(&pkt);
                break;
            }
            EncodedPacket ep;
            ep.packet = pkt;
            while (!m_outQueue.push(std::move(ep))) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
}

void VideoEncoder::encodingLoop() {
    while (m_running.load()) {
        auto maybeFrame = m_inQueue.pop();
        if (!maybeFrame.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        DecodedFrame df = std::move(maybeFrame.value());
        if (!df.frame) {
            continue;
        }

        int ret = avcodec_send_frame(m_codecCtx, df.frame);
        if (ret < 0) {
            LOG_ERROR("Video Encoder: Error sending frame to encoder.");
            av_frame_free(&df.frame);
            continue;
        }

        AVPacket* pkt = av_packet_alloc();
        while (true) {
            ret = avcodec_receive_packet(m_codecCtx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_free(&pkt);
                break;
            } else if (ret < 0) {
                LOG_ERROR("Video Encoder: Error receiving packet from encoder.");
                av_packet_free(&pkt);
                break;
            }

            EncodedPacket ep;
            ep.packet = pkt;
            while (!m_outQueue.push(std::move(ep))) {
                LOG_WARNING("Video Encoder: Packet queue is full, waiting...");
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            // Allocate a new packet for next iteration
            pkt = av_packet_alloc();
        }

        av_frame_free(&df.frame);
    }
    m_running.store(false);
}
