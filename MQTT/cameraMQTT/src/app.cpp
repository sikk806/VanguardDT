#include "app.h"
#include "utils.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <optional>
#include <string>
#include <cctype>

static std::optional<std::string> jsonGetString(const std::string& s, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    auto kpos = s.find(pat);
    if (kpos == std::string::npos) return std::nullopt;

    auto colon = s.find(':', kpos + pat.size());
    if (colon == std::string::npos) return std::nullopt;

    auto q1 = s.find('"', colon + 1);
    if (q1 == std::string::npos) return std::nullopt;

    auto q2 = s.find('"', q1 + 1);
    if (q2 == std::string::npos) return std::nullopt;

    return s.substr(q1 + 1, q2 - (q1 + 1));
}

static std::optional<int> jsonGetInt(const std::string& s, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    auto kpos = s.find(pat);
    if (kpos == std::string::npos) return std::nullopt;

    auto colon = s.find(':', kpos + pat.size());
    if (colon == std::string::npos) return std::nullopt;

    size_t i = colon + 1;
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;

    bool neg = false;
    if (i < s.size() && s[i] == '-') { neg = true; i++; }

    size_t j = i;
    while (j < s.size() && std::isdigit((unsigned char)s[j])) j++;
    if (j == i) return std::nullopt;

    int v = std::stoi(s.substr(i, j - i));
    return neg ? -v : v;
}

App::App(AppConfig cfg)
    : cfg_(std::move(cfg)),
      http_(utils::expandUser(cfg_.img_dir.string()), cfg_.http_port),
      mqtt_(cfg_.client_id, cfg_.mqtt_host, cfg_.mqtt_port),
      camera_(utils::expandUser(cfg_.img_dir.string()),
              cfg_.cam_device_index, cfg_.cam_width, cfg_.cam_height)
{
    // MQTT msg handler
    mqtt_.setMessageHandler([this](const std::string& topic, const std::string& payload){
        onMqttMessage(topic, payload);
    });

    // 카메라 shot 생성되면 -> image_url 만들어서 publish
    camera_.setShotCallback([this](uint64_t id, const std::string& filename){
        std::string myIp = utils::getMyIpBestEffort();
        std::ostringstream url;
        url << "http://" << myIp << ":" << cfg_.http_port << "/" << filename;
        mqtt_.publishCameraShot(id, url.str());
    });
}

void App::requestStop() {
    exit_.store(true);
    camera_.stopCapture();
}

int App::run() {
    auto imgDir = utils::expandUser(cfg_.img_dir.string());
    utils::ensureDir(imgDir);

    if (!http_.start()) {
        std::cerr << "[ERR] failed to start http server\n";
        return 1;
    }
    if (!mqtt_.startAndLoop()) {
        std::cerr << "[ERR] failed to start mqtt\n";
        return 1;
    }

    camera_.startThread();

    std::cerr << "[OK] running\n";
    while (!exit_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    camera_.stopCapture();
    camera_.joinThread();
    mqtt_.stop();
    http_.stop();

    std::cerr << "[BYE] exit\n";
    return 0;
}

void App::onMqttMessage(const std::string& topic, const std::string& payload) {
    if (topic != "robot/camera_cmd") return;

    auto action = jsonGetString(payload, "action");
    auto period = jsonGetInt(payload, "period_ms");

    if (action && *action == "start") {
        int p = period.value_or(cfg_.default_period_ms);
        p = std::clamp(p, 100, 60000);
        camera_.startCapture(p);
        std::cerr << "[CMD] camera start period_ms=" << p << "\n";
    } else if (action && *action == "stop") {
        camera_.stopCapture();
        std::cerr << "[CMD] camera stop\n";
    } else {
        std::cerr << "[CMD] unknown camera_cmd payload=" << payload << "\n";
    }
}
