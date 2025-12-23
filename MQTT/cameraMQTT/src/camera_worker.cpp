#include "camera_worker.h"
#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

CameraWorker::CameraWorker(std::filesystem::path imgDir,
                           int camDeviceIndex,
                           int camWidth,
                           int camHeight)
    : imgDir_(std::move(imgDir)),
      camDeviceIndex_(camDeviceIndex),
      camWidth_(camWidth),
      camHeight_(camHeight) {}

CameraWorker::~CameraWorker() {
    exit_.store(true);
    running_.store(false);
    cv_.notify_all();
    joinThread();
}

void CameraWorker::setShotCallback(ShotCallback cb) {
    onShot_ = std::move(cb);
}

void CameraWorker::startThread() {
    if (th_.joinable()) return;
    th_ = std::thread(&CameraWorker::threadMain, this);
}

void CameraWorker::joinThread() {
    if (th_.joinable()) th_.join();
}

void CameraWorker::startCapture(int period_ms) {
    periodMs_.store(std::max(100, period_ms));
    running_.store(true);
    cv_.notify_all();
}

void CameraWorker::stopCapture() {
    running_.store(false);
    cv_.notify_all();
}

void CameraWorker::threadMain() {
    cv::VideoCapture cap;
    bool opened = false;

    while (!exit_.load()) {
        // wait until running == true
        if (!running_.load()) {
            if (opened) {
                cap.release();
                opened = false;
                std::cerr << "[CAM] released\n";
            }
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&]{ return exit_.load() || running_.load(); });
            continue;
        }

        // open on demand
        if (!opened) {
            opened = cap.open(camDeviceIndex_, cv::CAP_V4L2);
            if (!opened) {
                std::cerr << "[CAM] open failed (device " << camDeviceIndex_ << ")\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            cap.set(cv::CAP_PROP_FRAME_WIDTH,  camWidth_);
            cap.set(cv::CAP_PROP_FRAME_HEIGHT, camHeight_);
            std::cerr << "[CAM] opened\n";
        }

        // capture loop
        while (!exit_.load() && running_.load()) {
            auto period = std::chrono::milliseconds(std::max(100, periodMs_.load()));

            cv::Mat frame;
            if (!cap.read(frame) || frame.empty()) {
                std::cerr << "[CAM] read failed\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            uint64_t id = ++shotId_;
            char fname[64];
            std::snprintf(fname, sizeof(fname), "shot_%06llu.jpg",
                          (unsigned long long)id);

            std::filesystem::path out = imgDir_ / fname;
            try {
                cv::imwrite(out.string(), frame);
            } catch (const std::exception& e) {
                std::cerr << "[CAM] imwrite exception: " << e.what() << "\n";
            }

            if (onShot_) onShot_(id, fname);

            // stop이 오면 즉시 깨우려고 cv로 wait
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait_for(lk, period, [&]{
                    return exit_.load() || !running_.load();
                });
            }
        }
    }

    if (opened) cap.release();
}
