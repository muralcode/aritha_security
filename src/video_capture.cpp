// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.
#include <video_capture.hpp>
#include <motion_detector.hpp> // for DecodedFrame
#include <chrono>
#include <thread>

VideoCapture::VideoCapture(const std::string& inputUrl,
                           BufferQueue<DecodedFrame, 128>& captureQueue,
                           bool reconnectOnFailure,
                           int reconnectDelaySecs)
    : m_inputUrl(inputUrl)
    , m_reconnectOnFailure(reconnectOnFailure)
    , m_reconnectDelaySecs(reconnectDelaySecs)
    , m_captureQueue(captureQueue)
{
    avformat_network_init();
}

VideoCapture::~VideoCapture() {
    stop();
    closeStream();
}

void VideoCapture::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&VideoCapture::captureLoop, this);
}

void VideoCapture::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool VideoCapture::openStream() {
    closeStream();

    int ret = avformat_open_input(&m_fmtCtx, m_inputUrl.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOG_ERROR("VideoCapture: Failed to open input: " + m_inputUrl);
        return false;
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        LOG_ERROR("VideoCapture: Failed to find stream info: " + m_inputUrl);
        return false;
    }

    m_videoStreamIndex = -1;
    for (unsigned int i = 0; i < m_fmtCtx->nb_streams; i++) {
        if (m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }
    if (m_videoStreamIndex < 0) {
        LOG_ERROR("VideoCapture: No video stream found in: " + m_inputUrl);
        return false;
    }

    AVCodecParameters* codecPar = m_fmtCtx->streams[m_videoStreamIndex]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        LOG_ERROR("VideoCapture: Decoder not found for: " + m_inputUrl);
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtx, codecPar);

    if ((ret = avcodec_open2(m_codecCtx, codec, nullptr)) < 0) {
        LOG_ERROR("VideoCapture: Failed to open codec for: " + m_inputUrl);
        return false;
    }

    LOG_INFO("VideoCapture: Successfully opened stream: " + m_inputUrl);
    return true;
}

void VideoCapture::closeStream() {
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }
}

void VideoCapture::captureLoop() {
    // Attempt initial open
    if (!openStream()) {
        if (!m_reconnectOnFailure) {
            m_running.store(false);
            return;
        }
    }

    AVPacket* packet = av_packet_alloc();
    while (m_running.load()) {
        if (!m_fmtCtx || !m_codecCtx) {
            if (m_reconnectOnFailure) {
                LOG_WARNING("VideoCapture: Trying reconnect in " + std::to_string(m_reconnectDelaySecs) + "s...");
                std::this_thread::sleep_for(std::chrono::seconds(m_reconnectDelaySecs));
                if (!openStream()) {
                    continue;
                }
            } else {
                break;
            }
        }

        int ret = av_read_frame(m_fmtCtx, packet);
        if (ret < 0) {
            LOG_WARNING("VideoCapture: av_read_frame returned " + std::to_string(ret));
            av_packet_unref(packet);
            if (m_reconnectOnFailure) {
                closeStream();
                continue;
            } else {
                break;
            }
        }

        if (packet->stream_index == m_videoStreamIndex) {
            ret = avcodec_send_packet(m_codecCtx, packet);
            if (ret < 0) {
                LOG_ERROR("VideoCapture: Error sending packet for decode.");
                av_packet_unref(packet);
                continue;
            }

            while (true) {
                AVFrame* frame = av_frame_alloc();
                ret = avcodec_receive_frame(m_codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_frame_free(&frame);
                    break;
                } else if (ret < 0) {
                    LOG_ERROR("VideoCapture: Error decoding frame.");
                    av_frame_free(&frame);
                    break;
                }

                // Push decoded frame
                DecodedFrame df;
                df.frame = frame;
                df.pts = frame->pts;

                while (!m_captureQueue.push(std::move(df))) {
                    LOG_WARNING("VideoCapture: capture queue full, dropping frame...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    closeStream();
    m_running.store(false);
}
