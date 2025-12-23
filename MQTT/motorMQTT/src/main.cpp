#include <rclcpp/rclcpp.hpp>
#include "motor_bridge_node.h"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<MotorBridgeNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
