#pragma once
#include <mosquitto.h>
#include <atomic>
#include <string>

class MqttPublisher {
public:
    MqttPublisher(std::string clientId, std::string host, int port);
    ~MqttPublisher();

    bool startAndLoop();
    void stop();

    bool isConnected() const { return connected_.load(); }
    void publishMapState(const std::string& jsonPayload);

private:
    std::string clientId_;
    std::string host_;
    int port_;

    mosquitto* mosq_{nullptr};
    std::atomic<bool> connected_{false};

    static void on_connect(struct mosquitto*, void* obj, int rc);
    static void on_disconnect(struct mosquitto*, void* obj, int rc);

    void handleConnect(int rc);
    void handleDisconnect(int rc);
};
