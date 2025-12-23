cmake_minimum_required(VERSION 3.14)
project(ros_motor_uart_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)

option(USE_MOSQUITTO "Enable MQTT publish via libmosquitto" ON)

if(USE_MOSQUITTO)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(MOSQUITTO libmosquitto)
  if(NOT MOSQUITTO_FOUND)
    message(WARNING "libmosquitto not found. MQTT publish will be disabled.")
    set(USE_MOSQUITTO OFF)
  else()
    add_compile_definitions(USE_MOSQUITTO)
  endif()
endif()

include_directories(include)

add_library(bridge_core
  src/utils.cpp
  src/mqtt_pub.cpp
  src/uart_port.cpp
  src/motor_bridge_node.cpp
)

ament_target_dependencies(bridge_core
  rclcpp
  geometry_msgs
  std_msgs
)

if(USE_MOSQUITTO)
  target_include_directories(bridge_core PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
  target_link_libraries(bridge_core ${MOSQUITTO_LIBRARIES})
endif()

add_executable(motor_bridge_node
  src/main.cpp
)
target_link_libraries(motor_bridge_node bridge_core)
ament_target_dependencies(motor_bridge_node rclcpp)

add_executable(auto_test
  src/auto_test.cpp
  src/uart_port.cpp
  src/utils.cpp
)
ament_target_dependencies(auto_test rclcpp)

install(TARGETS
  motor_bridge_node
  auto_test
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
