#pragma once

#include "app_config.h" // Config 구조체 정의된 헤더 (기존 코드에 없으면 따로 정의 필요)
#include "http_server.h"
#include "mqtt_client.h"
#include "camera_worker.h"

// [추가] ROS 2 관련 헤더
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <nav_msgs/msg/map_meta_data.hpp>

#include <atomic>
#include <memory>

// [수정] rclcpp::Node 상속
class App : public rclcpp::Node {
public:
    explicit App(AppConfig cfg);
    ~App() override = default;

    int run();
    void requestStop();

private:
    void onMqttMessage(const std::string& topic, const std::string& payload);
    
    void getCurrentRobotPose(double& x, double& y, double& theta);

    void onMapMetadata(const nav_msgs::msg::MapMetaData::SharedPtr msg);

    void applyMapOriginCorrection(double wx, double wy, double& out_x, double& out_y);
    
    AppConfig cfg_;
    std::atomic<bool> exit_{false};

    HttpServer http_;
    MqttClient mqtt_;
    CameraWorker camera_;

    // [추가] TF 관련 멤버
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

    rclcpp::Subscription<nav_msgs::msg::MapMetaData>::SharedPtr map_sub_;
    nav_msgs::msg::MapMetaData current_map_info_;
    bool has_map_info_ = false;
    std::mutex map_mtx_;
};
