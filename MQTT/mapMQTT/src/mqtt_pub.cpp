#include "mqtt_pub.h"
#include <iostream>

MqttPublisher::MqttPublisher(std::string clientId, std::string host, int port)
    : clientId_(std::move(clientId)), host_(std::move(host)), port_(port) {}

MqttPublisher::~MqttPublisher() { stop(); }

bool MqttPublisher::startAndLoop() {
    mosquitto_lib_init();

    mosq_ = mosquitto_new(clientId_.c_str(), true, this);
    if (!mosq_) {
        std::cerr << "[ERR] mosquitto_new failed\n";
        mosquitto_lib_cleanup();
        return false;
    }

    mosquitto_connect_callback_set(mosq_, &MqttPublisher::on_connect);
    mosquitto_disconnect_callback_set(mosq_, &MqttPublisher::on_disconnect);

    int rc = mosquitto_connect_async(mosq_, host_.c_str(), port_, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] connect_async failed rc=" << rc << "\n";
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        mosquitto_lib_cleanup();
        return false;
    }

    rc = mosquitto_loop_start(mosq_);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] loop_start failed rc=" << rc << "\n";
        mosquitto_disconnect(mosq_);
        mosquitto_destroy(mosq_);
        mosq_ = nullptr;
        mosquitto_lib_cleanup();
        return false;
    }

    return true;
}

void MqttPublisher::stop() {
    if (!mosq_) return;

    mosquitto_loop_stop(mosq_, true);
    mosquitto_disconnect(mosq_);
    mosquitto_destroy(mosq_);
    mosq_ = nullptr;

    mosquitto_lib_cleanup();
    connected_.store(false);
}

void MqttPublisher::publishMapState(const std::string& jsonPayload) {
    if (!mosq_ || !connected_.load()) return;

    int rc = mosquitto_publish(mosq_, nullptr,
                              "robot/map_state",
                              (int)jsonPayload.size(), jsonPayload.data(),
                              0, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] publish map_state failed rc=" << rc << "\n";
    }
}

void MqttPublisher::on_connect(struct mosquitto*, void* obj, int rc) {
    static_cast<MqttPublisher*>(obj)->handleConnect(rc);
}
void MqttPublisher::on_disconnect(struct mosquitto*, void* obj, int rc) {
    static_cast<MqttPublisher*>(obj)->handleDisconnect(rc);
}

void MqttPublisher::handleConnect(int rc) {
    if (rc == 0) {
        connected_.store(true);
        std::cerr << "[MQTT] connected\n";
    } else {
        std::cerr << "[MQTT] connect failed rc=" << rc << "\n";
    }
}
void MqttPublisher::handleDisconnect(int) {
    connected_.store(false);
    std::cerr << "[MQTT] disconnected\n";
}
