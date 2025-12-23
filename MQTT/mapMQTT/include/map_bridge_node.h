#pragma once
#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>

#include "http_server.h"
#include "mqtt_pub.h"

#include <filesystem>
#include <atomic>

class MapBridgeNode : public rclcpp::Node {
public:
    MapBridgeNode();

private:
    // params
    std::string mqtt_host_;
    int mqtt_port_;
    std::string mqtt_client_id_;

    int http_port_;
    std::filesystem::path out_dir_;
    double publish_hz_;  // throttle (e.g., 2.0)

    // components
    HttpServer http_;
    MqttPublisher mqtt_;

    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr sub_;
    rclcpp::Time last_pub_time_{0, 0, RCL_ROS_TIME};

    std::atomic<uint64_t> map_seq_{0};

    void onMap(const nav_msgs::msg::OccupancyGrid::SharedPtr msg);

    static double yawFromQuat(const geometry_msgs::msg::Quaternion& q);
    static uint8_t occToGray(int8_t v);
};
