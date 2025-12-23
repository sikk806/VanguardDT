cmake_minimum_required(VERSION 3.10)
project(ros_map_mqtt_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(map_mqtt_bridge
  src/main.cpp
  src/map_bridge_node.cpp
  src/utils.cpp
  src/http_server.cpp
  src/mqtt_pub.cpp
)

target_include_directories(map_mqtt_bridge PRIVATE include)

ament_target_dependencies(map_mqtt_bridge
  rclcpp
  nav_msgs
  geometry_msgs
  tf2
)

target_link_libraries(map_mqtt_bridge
  ${OpenCV_LIBS}
  mosquitto
  pthread
)

install(TARGETS map_mqtt_bridge
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
