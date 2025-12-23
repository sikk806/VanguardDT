#include <rclcpp/rclcpp.hpp>
#include "map_bridge_node.h"

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<MapBridgeNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
