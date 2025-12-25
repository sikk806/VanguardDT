#include "app.h"
#include "utils.h"

#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/LinearMath/Quaternion.h>
#include <geometry_msgs/msg/transform_stamped.hpp>

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
    : Node("camera_app_node"),
      cfg_(std::move(cfg)),
      http_(utils::expandUser(cfg_.img_dir.string()), cfg_.http_port),
      mqtt_(cfg_.client_id, cfg_.mqtt_host, cfg_.mqtt_port),
      camera_(utils::expandUser(cfg_.img_dir.string()),
              cfg_.cam_device_index, cfg_.cam_width, cfg_.cam_height)
{
    tf_buffer_ = std::make_shared<tf2_ros::Buffer>(this->get_clock(), std::chrono::seconds(10));
    tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

    // [추가] 지도 메타데이터 구독 (QoS는 Reliable + Transient Local 추천)
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).transient_local().reliable();
    map_sub_ = this->create_subscription<nav_msgs::msg::MapMetaData>(
        "/map_metadata", qos,
        std::bind(&App::onMapMetadata, this, std::placeholders::_1));

    mqtt_.setMessageHandler([this](const std::string& topic, const std::string& payload){
        onMqttMessage(topic, payload);
    });

    camera_.setShotCallback([this](uint64_t id, const std::string& filename){
        std::string myIp = utils::getMyIpBestEffort();
        std::ostringstream url;
        url << "http://" << myIp << ":" << cfg_.http_port << "/" << filename;
        
        // 1. 로봇의 World 좌표(m) 구하기
        double wx = 0.0, wy = 0.0, th = 0.0;
        getCurrentRobotPose(wx, wy, th);

        // 2. [핵심] 픽셀 좌표(px)로 변환하기
        double corrected_x = 0.0, corrected_y = 0.0;
	applyMapOriginCorrection(wx, wy, corrected_x, corrected_y);

        // 3. 변환된 픽셀 좌표를 MQTT로 전송 (x, y 자리에 px, py를 넣음)
        // 받는 쪽(웹)에서는 이제 이 값을 그대로 이미지 위에 찍으면 됨
        mqtt_.publishCameraShot(id, url.str(), corrected_x, corrected_y, th);
        
        // 디버깅 로그
    RCLCPP_INFO(this->get_logger(), 
            "📸 Shot! RViz(%.2f, %.2f) -> Corrected(%.2f, %.2f)", 
            wx, wy, corrected_x, corrected_y);
	});
}

// [추가] 지도 정보 콜백
void App::onMapMetadata(const nav_msgs::msg::MapMetaData::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(map_mtx_);
    current_map_info_ = *msg;
    has_map_info_ = true;
}

void App::applyMapOriginCorrection(double wx, double wy, double& out_x, double& out_y) {
    std::lock_guard<std::mutex> lock(map_mtx_);
    
    if (!has_map_info_) {
        // 지도 정보가 아직 없으면 그냥 RViz 좌표 그대로 보냄
        out_x = wx;
        out_y = wy;
        return;
    }

    // 맵 파일(.yaml)에 적힌 원점 좌표 (보통 지도의 왼쪽 아래 구석)
    double origin_x = current_map_info_.origin.position.x;
    double origin_y = current_map_info_.origin.position.y;

    // 현재 위치 - 원점 위치 = "원점으로부터 얼만큼 떨어져 있는지(m)"
    out_x = wx - origin_x;
    out_y = wy - origin_y;
}

void App::getCurrentRobotPose(double& x, double& y, double& theta) {
    try {
        geometry_msgs::msg::TransformStamped t;
        if (tf_buffer_->canTransform("map", "base_link", tf2::TimePointZero)) {
            t = tf_buffer_->lookupTransform("map", "base_link", tf2::TimePointZero);
        } else {
            t = tf_buffer_->lookupTransform("odom", "base_link", tf2::TimePointZero);
        }

        x = t.transform.translation.x;
        y = t.transform.translation.y;

        tf2::Quaternion q(
            t.transform.rotation.x,
            t.transform.rotation.y,
            t.transform.rotation.z,
            t.transform.rotation.w);
        tf2::Matrix3x3 m(q);
        double roll, pitch, yaw;
        m.getRPY(roll, pitch, yaw);
        theta = yaw;
    } catch (const tf2::TransformException &ex) {
        x = 0.0; y = 0.0; theta = 0.0;
    }
}

void App::requestStop() {
    exit_.store(true);
    camera_.stopCapture();
    rclcpp::shutdown();
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
	rclcpp::spin_some(this->shared_from_this()); // 콜백 처리
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
