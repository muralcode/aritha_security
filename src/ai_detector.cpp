// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.
// The parsing logic in runInference() depends on your model’s architecture. 
// If you have a different model (like SSD, Faster R-CNN, or a custom ONNX), adjust accordingly. 

#include <ai_detector.hpp>
#include <chrono>
#include <thread>

AIDetector::AIDetector(BufferQueue<DecodedFrame, 128>& inQueue,
                       BufferQueue<DecodedFrame, 128>& outQueue,
                       const std::string& modelPath,
                       const std::string& modelConfig,
                       bool useGPU)
    : m_inQueue(inQueue)
    , m_outQueue(outQueue)
    , m_useGPU(useGPU)
{
    if (!loadModel(modelPath, modelConfig)) {
        LOG_ERROR("AIDetector: Failed to load model!");
    }
}

AIDetector::~AIDetector() {
    stop();
}

bool AIDetector::loadModel(const std::string& modelPath, const std::string& modelConfig) {
    try {
        if (!modelConfig.empty()) {
            // For frameworks like Caffe (prototxt + caffemodel) or TensorFlow (pb + pbtxt)
            m_net = cv::dnn::readNet(modelPath, modelConfig);
        } else {
            // For ONNX or single-file models
            m_net = cv::dnn::readNet(modelPath);
        }

        if (m_useGPU) {
#ifdef CV_CUDNN
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            LOG_INFO("AIDetector: Using GPU inference (CUDA).");
#else
            LOG_WARNING("AIDetector: GPU requested but OpenCV built without CUDA. Falling back to CPU.");
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
#endif
        } else {
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            LOG_INFO("AIDetector: Using CPU inference.");
        }
    } catch (const cv::Exception& e) {
        LOG_ERROR(std::string("AIDetector: Exception loading model: ") + e.what());
        return false;
    }

    LOG_INFO("AIDetector: Model loaded successfully.");
    return true;
}

void AIDetector::start() {
    if (m_running.load()) return;
    m_running.store(true);
    m_thread = std::thread(&AIDetector::detectionLoop, this);
}

void AIDetector::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void AIDetector::detectionLoop() {
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

        // Convert to BGR
        cv::Mat bgr = avFrameToMat(df.frame);

        // Run inference
        auto detections = runInference(bgr);
        // (Optional) Draw bounding boxes
        for (auto& det : detections) {
            cv::rectangle(bgr,
                cv::Rect(det.x, det.y, det.width, det.height),
                cv::Scalar(0, 255, 0),
                2
            );
            // Could also draw labels, etc.
        }

        // If you want to encode the bounding boxes “burned in”, you must convert BGR back to YUV
        // That’s extra overhead. Alternatively, do nothing and just send the original frame to outQueue.

        while (!m_outQueue.push(std::move(df))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    m_running.store(false);
}

cv::Mat AIDetector::avFrameToMat(AVFrame* frame) {
    // frame->format is likely AV_PIX_FMT_YUV420P from your pipeline
    // We need to convert to BGR for OpenCV DNN
    // Basic approach: use libswscale (SwsContext) or do a manual approach if you prefer.

    // We'll do a quick approach using cv::Mat to wrap data:
    // This is tricky because AVFrame is planar (Y, U, V planes).
    // Easiest is to do a sws_scale. For brevity, we’ll do a naive approach:

    cv::Mat bgr;
    // Lerato: allocate a new AVFrame or a buffer, then sws_scale to get BGR24, then wrap in cv::Mat.
    // For a real solution, create a SwsContext once in your constructor, reuse it for performance.

    // Because we want to keep code simpler, let's do a naive approach with a known function:
    static struct SwsContext* swsCtx = nullptr;

    if (!swsCtx) {
        swsCtx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,
            frame->width, frame->height, AV_PIX_FMT_BGR24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
    }

    cv::Mat temp(frame->height, frame->width, CV_8UC3); // BGR24
    uint8_t* dest[4] = { temp.data, nullptr, nullptr, nullptr };
    int destLinesize[4] = { static_cast<int>(frame->width * 3), 0, 0, 0 };

    sws_scale(swsCtx,
              frame->data, frame->linesize,
              0, frame->height,
              dest, destLinesize);

    bgr = temp.clone(); // copy out if needed
    return bgr;
}

std::vector<DetectionBox> AIDetector::runInference(const cv::Mat& bgr) {
    std::vector<DetectionBox> results;

    if (m_net.empty()) {
        LOG_ERROR("AIDetector: Net is empty, can't run inference!");
        return results;
    }

    // The specifics here depend on your model:
    // 1. Create a blob
    cv::Mat blob = cv::dnn::blobFromImage(bgr, 1.0/255.0, cv::Size(640, 640), cv::Scalar(), true, false);
    m_net.setInput(blob);

    // 2. Forward pass
    cv::Mat output = m_net.forward();

    // 3. Parse output
    // The parsing logic depends on your specific model’s output layout.
    // For a YOLO-like model, you might have columns: [x_center, y_center, w, h, conf, classScores...]
    // This is just a placeholder example.

    const int rows = output.size[1];
    const int cols = output.size[2];

    float* data = (float*)output.data;
    for (int i = 0; i < rows; i++) {
        float confidence = data[4];
        if (confidence < 0.5f) { // threshold
            data += cols;
            continue;
        }
        // parse box
        float x = data[0];
        float y = data[1];
        float w = data[2];
        float h = data[3];
        DetectionBox db;
        db.x = (x - w/2) * bgr.cols;   // scale to image coords
        db.y = (y - h/2) * bgr.rows;
        db.width  = w * bgr.cols;
        db.height = h * bgr.rows;
        db.confidence = confidence;
        db.classId = (int)data[5]; // example

        results.push_back(db);
        data += cols;
    }

    return results;
}
