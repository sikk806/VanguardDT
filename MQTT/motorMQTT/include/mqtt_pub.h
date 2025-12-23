#pragma once
#include <string>
#include <cstdint>

class MqttPublisher {
public:
    MqttPublisher();
    ~MqttPublisher();

    bool connect(const std::string& host, int port, const std::string& client_id);
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retain = false);
    void disconnect();

private:
#ifdef USE_MOSQUITTO
    struct mosquitto* mosq_ = nullptr;
    bool connected_ = false;
    std::string host_;
    int port_ = 1883;
#endif
};
