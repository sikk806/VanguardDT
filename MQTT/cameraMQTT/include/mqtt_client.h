#pragma once
#include <mosquitto.h>
#include <atomic>
#include <functional>
#include <random>
#include <string>

class MqttClient {
public:
    using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;

    MqttClient(std::string clientId, std::string host, int port);
    ~MqttClient();

    void setMessageHandler(MessageHandler h);

    bool startAndLoop(); // connect + loop_start
    void stop();

    bool isConnected() const { return connected_.load(); }

    // 기존 코드처럼 x,y,theta 랜덤으로 넣어서 camera_shot 발행
    void publishCameraShot(uint64_t shotNum, const std::string& imageUrl);

private:
    std::string clientId_;
    std::string host_;
    int port_;

    mosquitto* mosq_{nullptr};
    std::atomic<bool> connected_{false};
    MessageHandler handler_;

    std::mt19937 rng_{ std::random_device{}() };

    static void on_connect(struct mosquitto*, void* obj, int rc);
    static void on_disconnect(struct mosquitto*, void* obj, int rc);
    static void on_message(struct mosquitto*, void* obj, const struct mosquitto_message* msg);

    void handleConnect(int rc);
    void handleDisconnect(int rc);
    void handleMessage(const struct mosquitto_message* msg);

    static std::string makeShotId(uint64_t id);
};
