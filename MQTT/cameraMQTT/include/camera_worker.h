#pragma once
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>

class CameraWorker {
public:
    using ShotCallback = std::function<void(uint64_t shotNum, const std::string& filename)>;

    CameraWorker(std::filesystem::path imgDir,
                 int camDeviceIndex,
                 int camWidth,
                 int camHeight);

    ~CameraWorker();

    void startThread();
    void joinThread();

    void startCapture(int period_ms);
    void stopCapture();

    void setShotCallback(ShotCallback cb);

private:
    std::filesystem::path imgDir_;
    int camDeviceIndex_;
    int camWidth_;
    int camHeight_;

    std::atomic<bool> exit_{false};
    std::atomic<bool> running_{false};
    std::atomic<int> periodMs_{1000};
    std::atomic<uint64_t> shotId_{0};

    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread th_;

    ShotCallback onShot_;

    void threadMain();
};
