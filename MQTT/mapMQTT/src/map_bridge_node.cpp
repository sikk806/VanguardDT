#include "map_bridge_node.h"
#include "utils.h"

#include <geometry_msgs/msg/quaternion.hpp>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/LinearMath/Quaternion.h>

#include <opencv2/opencv.hpp>

#include <iomanip>
#include <sstream>

MapBridgeNode::MapBridgeNode()
    : Node("map_mqtt_bridge"),
      mqtt_host_(this->declare_parameter<std::string>("mqtt_host", "127.0.0.1")),
      mqtt_port_(this->declare_parameter<int>("mqtt_port", 1883)),
      mqtt_client_id_(this->declare_parameter<std::string>("mqtt_client_id", "ros_map_mqtt_bridge")),
      http_port_(this->declare_parameter<int>("http_port", 8001)),
      out_dir_(utils::expandUser(this->declare_parameter<std::string>("out_dir", "~/maps/map_images"))),
      publish_hz_(this->declare_parameter<double>("publish_hz", 2.0)),
      http_(out_dir_, http_port_),
      mqtt_(mqtt_client_id_, mqtt_host_, mqtt_port_)
{
    utils::ensureDir(out_dir_);

    if (!http_.start()) {
        RCLCPP_ERROR(this->get_logger(), "Failed to start HTTP server");
    }
    if (!mqtt_.startAndLoop()) {
        RCLCPP_ERROR(this->get_logger(), "Failed to start MQTT");
    }

    // /map은 보통 "latched(transient_local)"로 오므로 QoS를 맞추는 게 안전
    auto qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();

    sub_ = this->create_subscription<nav_msgs::msg::OccupancyGrid>(
        "/map", qos,
        std::bind(&MapBridgeNode::onMap, this, std::placeholders::_1)
    );

    RCLCPP_INFO(this->get_logger(), "Started. Saving maps to: %s", out_dir_.c_str());
}

double MapBridgeNode::yawFromQuat(const geometry_msgs::msg::Quaternion& q) {
    tf2::Quaternion tq(q.x, q.y, q.z, q.w);
    double roll = 0.0, pitch = 0.0, yaw = 0.0;
    tf2::Matrix3x3(tq).getRPY(roll, pitch, yaw);
    return yaw;
}

// map_server 관례에 맞춘 그레이 값:
// unknown(-1)=205, free(0)=254, occupied(100)=0, 중간은 선형 보간
uint8_t MapBridgeNode::occToGray(int8_t v) {
    if (v < 0) return 205;
    if (v > 100) v = 100;
    // 0->254, 100->0
    int gray = (100 - (int)v) * 254 / 100;
    if (gray < 0) gray = 0;
    if (gray > 255) gray = 255;
    return (uint8_t)gray;
}

void MapBridgeNode::onMap(const nav_msgs::msg::OccupancyGrid::SharedPtr msg) {
    if (!msg) return;

    // throttle
    const double min_dt = (publish_hz_ <= 0.0) ? 0.0 : (1.0 / publish_hz_);
    auto now = this->get_clock()->now();
    if (last_pub_time_.nanoseconds() != 0) {
        double dt = (now - last_pub_time_).seconds();
        if (dt < min_dt) return;
    }
    last_pub_time_ = now;

    const auto& info = msg->info;
    const int w = (int)info.width;
    const int h = (int)info.height;
    if (w <= 0 || h <= 0) return;
    if ((int)msg->data.size() < w * h) return;

    // OccupancyGrid는 "origin이 맵의 (0,0) 위치"이고, data는 row-major
    // 보통 GUI에서 보기 편하게 하려면 y축을 뒤집어서 저장하는 경우가 많음.
    // 여기서는 "map_server가 만든 pgm 보는 느낌"으로 y-flip해서 저장.
    cv::Mat img(h, w, CV_8UC1);
    for (int y = 0; y < h; ++y) {
        int yy = (h - 1 - y); // flip
        uint8_t* row = img.ptr<uint8_t>(y);
        for (int x = 0; x < w; ++x) {
            int idx = yy * w + x;
            row[x] = occToGray(msg->data[idx]);
        }
    }

    uint64_t seq = ++map_seq_;
    std::ostringstream idss;
    idss << "map_" << std::setw(6) << std::setfill('0') << seq;
    std::string map_id = idss.str();

    std::ostringstream fnss;
    fnss << "map_" << std::setw(6) << std::setfill('0') << seq << ".png";
    std::string filename = fnss.str();

    std::filesystem::path out = out_dir_ / filename;
    try {
        cv::imwrite(out.string(), img);
    } catch (const std::exception& e) {
        RCLCPP_ERROR(this->get_logger(), "imwrite failed: %s", e.what());
        return;
    }

    // URL 생성
    std::string ip = utils::getMyIpBestEffort();
    std::ostringstream url;
    url << "http://" << ip << ":" << http_port_ << "/" << filename;

    // metadata
    double res = info.resolution;
    double ox = info.origin.position.x;
    double oy = info.origin.position.y;
    double oyaw = yawFromQuat(info.origin.orientation);

    // MQTT payload (명세에 맞게 키 이름 바꿔도 됨)
    std::ostringstream payload;
    payload << "{"
            << "\"map_id\":\"" << map_id << "\","
            << "\"image_url\":\"" << url.str() << "\","
            << "\"width\":" << w << ","
            << "\"height\":" << h << ","
            << "\"resolution\":" << res << ","
            << "\"origin\":[" << ox << "," << oy << "," << oyaw << "],"
            << "\"timestamp\":\"" << utils::nowIsoUtcZ() << "\""
            << "}";

    mqtt_.publishMapState(payload.str());

    RCLCPP_INFO(this->get_logger(),
                "Published map_state: %s (%dx%d res=%.3f url=%s)",
                map_id.c_str(), w, h, res, url.str().c_str());
}
