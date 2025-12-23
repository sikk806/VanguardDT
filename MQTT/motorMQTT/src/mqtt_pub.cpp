#include "mqtt_pub.h"
#include <iostream>

#ifdef USE_MOSQUITTO
#include <mosquitto.h>
#endif

MqttPublisher::MqttPublisher() {
#ifdef USE_MOSQUITTO
    mosquitto_lib_init();
#endif
}

MqttPublisher::~MqttPublisher() {
    disconnect();
#ifdef USE_MOSQUITTO
    mosquitto_lib_cleanup();
#endif
}

bool MqttPublisher::connect(const std::string& host, int port, const std::string& client_id) {
#ifdef USE_MOSQUITTO
    host_ = host;
    port_ = port;

    mosq_ = mosquitto_new(client_id.c_str(), true, nullptr);
    if (!mosq_) {
        std::cerr << "[mqtt] mosquitto_new failed\n";
        return false;
    }

    int rc = mosquitto_connect(mosq_, host.c_str(), port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[mqtt] connect failed: " << mosquitto_strerror(rc) << "\n";
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        return false;
    }

    rc = mosquitto_loop_start(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[mqtt] loop_start failed: " << mosquitto_strerror(rc) << "\n";
        mosquitto_disconnect(mosq_);
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        return false;
    }

    connected_ = true;
    std::cerr << "[mqtt] connected to " << host << ":" << port << "\n";
    return true;
#else
    (void)host; (void)port; (void)client_id;
    std::cerr << "[mqtt] USE_MOSQUITTO=OFF (disabled)\n";
    return false;
#endif
}

bool MqttPublisher::publish(const std::string& topic, const std::string& payload, int qos, bool retain) {
#ifdef USE_MOSQUITTO
    if (!mosq_ || !connected_) return false;

    int rc = mosquitto_publish(
        mosq_,
        nullptr,
        topic.c_str(),
        (int)payload.size(),
        payload.data(),
        qos,
        retain
    );

    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[mqtt] publish failed: " << mosquitto_strerror(rc) << "\n";
        return false;
    }
    return true;
#else
    (void)topic; (void)payload; (void)qos; (void)retain;
    return true; // no-op
#endif
}

void MqttPublisher::disconnect() {
#ifdef USE_MOSQUITTO
    if (!mosq_) return;

    if (connected_) {
        mosquitto_loop_stop(mosq_, true);
        mosquitto_disconnect(mosq_);
        connected_ = false;
    }
    mosquitto_destroy(mosq_);
    mosq_ = nullptr;
#endif
}
