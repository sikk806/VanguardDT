#include "mqtt_client.h"
#include "utils.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

MqttClient::MqttClient(std::string clientId, std::string host, int port)
    : clientId_(std::move(clientId)), host_(std::move(host)), port_(port) {}

MqttClient::~MqttClient() {
    stop();
}

void MqttClient::setMessageHandler(MessageHandler h) {
    handler_ = std::move(h);
}

bool MqttClient::startAndLoop() {
    mosquitto_lib_init();

    mosq_ = mosquitto_new(clientId_.c_str(), true, this);
    if (!mosq_) {
        std::cerr << "[ERR] mosquitto_new failed\n";
        mosquitto_lib_cleanup();
        return false;
    }

    mosquitto_connect_callback_set(mosq_, &MqttClient::on_connect);
    mosquitto_disconnect_callback_set(mosq_, &MqttClient::on_disconnect);
    mosquitto_message_callback_set(mosq_, &MqttClient::on_message);

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

void MqttClient::stop() {
    if (!mosq_) return;

    mosquitto_loop_stop(mosq_, true);
    mosquitto_disconnect(mosq_);
    mosquitto_destroy(mosq_);
    mosq_ = nullptr;

    mosquitto_lib_cleanup();
    connected_.store(false);
}

void MqttClient::publishCameraShot(uint64_t shotNum, const std::string& imageUrl) {
    if (!mosq_ || !connected_.load()) return;

    std::uniform_real_distribution<double> dx(-1.0, 1.0);
    std::uniform_real_distribution<double> dy(-1.0, 1.0);
    std::uniform_real_distribution<double> dtheta(-3.14159, 3.14159);

    double x = dx(rng_);
    double y = dy(rng_);
    double theta = dtheta(rng_);

    std::string shotId = makeShotId(shotNum);

    std::ostringstream oss;
    oss << "{"
        << "\"shot_id\":\"" << shotId << "\","
        << "\"x\":" << x << ","
        << "\"y\":" << y << ","
        << "\"theta\":" << theta << ","
        << "\"image_url\":\"" << imageUrl << "\","
        << "\"timestamp\":\"" << utils::nowIsoUtcZ() << "\""
        << "}";

    std::string payload = oss.str();
    int rc = mosquitto_publish(mosq_, nullptr,
                              "robot/camera_shot",
                              (int)payload.size(), payload.data(),
                              0, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[MQTT] publish failed rc=" << rc << "\n";
    }
}

std::string MqttClient::makeShotId(uint64_t id) {
    std::ostringstream oss;
    oss << "shot_" << std::setw(4) << std::setfill('0') << id;
    return oss.str();
}

void MqttClient::on_connect(struct mosquitto*, void* obj, int rc) {
    static_cast<MqttClient*>(obj)->handleConnect(rc);
}
void MqttClient::on_disconnect(struct mosquitto*, void* obj, int rc) {
    static_cast<MqttClient*>(obj)->handleDisconnect(rc);
}
void MqttClient::on_message(struct mosquitto*, void* obj, const struct mosquitto_message* msg) {
    static_cast<MqttClient*>(obj)->handleMessage(msg);
}

void MqttClient::handleConnect(int rc) {
    if (rc == 0) {
        connected_.store(true);
        std::cerr << "[MQTT] connected\n";
        mosquitto_subscribe(mosq_, nullptr, "robot/camera_cmd", 0);
    } else {
        std::cerr << "[MQTT] connect failed rc=" << rc << "\n";
    }
}
void MqttClient::handleDisconnect(int) {
    connected_.store(false);
    std::cerr << "[MQTT] disconnected\n";
}

void MqttClient::handleMessage(const struct mosquitto_message* msg) {
    if (!msg || !msg->topic || !msg->payload) return;
    if (!handler_) return;

    std::string topic = msg->topic;
    std::string payload((const char*)msg->payload, (size_t)msg->payloadlen);
    handler_(topic, payload);
}
