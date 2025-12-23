#pragma once
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <std_msgs/msg/bool.hpp>

#include "mqtt_pub.h"
#include "uart_port.h"

class MotorBridgeNode : public rclcpp::Node {
public:
    MotorBridgeNode();

private:
    void on_cmd_vel(const geometry_msgs::msg::Twist::SharedPtr msg);
    void on_enable(const std_msgs::msg::Bool::SharedPtr msg);
    void on_estop(const std_msgs::msg::Bool::SharedPtr msg);

    void on_timer();
    std::string build_motor_cmd_json(const rc_car::DriveCmd& cmd, bool enable, bool estop) const;

private:
    // params
    std::string uart_device_;
    int uart_baud_ = 115200;

    std::string mqtt_host_;
    int mqtt_port_ = 1883;
    std::string mqtt_topic_;

    std::string cmd_vel_topic_;
    std::string enable_topic_;
    std::string estop_topic_;

    double speed_gain_ = 80.0; // m/s -> percent
    double steer_gain_ = 60.0; // rad/s -> percent
    int send_rate_hz_ = 20;
    int cmd_timeout_ms_ = 300;
    bool start_stm_log_ = false;

    // runtime
    MqttPublisher mqtt_;
    geometry_msgs::msg::Twist last_twist_;
    rclcpp::Time last_cmd_time_;

    bool enable_ = true;
    bool estop_ = false;

    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_cmd_vel_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr sub_enable_;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr sub_estop_;
    rclcpp::TimerBase::SharedPtr timer_;
};
