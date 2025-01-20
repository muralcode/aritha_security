#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <buffer_queue.hpp>
#include <video_capture.hpp>  // for DecodedFrame
#include "logger.hpp"

// we create a simple struct to hold detection information.
struct DetectionBox {
    float x, y, width, height;
    float confidence;
    int classId;
};

class AIDetector {
public:
    AIDetector(BufferQueue<DecodedFrame, 128>& inQueue,
               BufferQueue<DecodedFrame, 128>& outQueue,
               const std::string& modelPath,
               const std::string& modelConfig,
               bool useGPU);
    ~AIDetector();

    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    void detectionLoop();
    bool loadModel(const std::string& modelPath, const std::string& modelConfig);

    // Convert AVFrame (YUV) to OpenCV Mat (BGR)
    cv::Mat avFrameToMat(AVFrame* frame);

    // Inference & post-processing
    std::vector<DetectionBox> runInference(const cv::Mat& bgr);

private:
    BufferQueue<DecodedFrame, 128>& m_inQueue;
    BufferQueue<DecodedFrame, 128>& m_outQueue;

    std::atomic<bool> m_running{false};
    std::thread m_thread;

    cv::dnn::Net m_net;
    bool m_useGPU;
};
