#pragma once
#include "app_config.h"
#include "http_server.h"
#include "mqtt_client.h"
#include "camera_worker.h"

#include <atomic>

class App {
public:
    explicit App(AppConfig cfg);
    int run();

    void requestStop(); // SIGINT/SIGTERM에서 호출

private:
    AppConfig cfg_;
    std::atomic<bool> exit_{false};

    HttpServer http_;
    MqttClient mqtt_;
    CameraWorker camera_;

    void onMqttMessage(const std::string& topic, const std::string& payload);
};