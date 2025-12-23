cmake_minimum_required(VERSION 3.16)
project(rpi_cam CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(rpi_cam
  src/main.cpp
  src/app.cpp
  src/utils.cpp
  src/http_server.cpp
  src/mqtt_client.cpp
  src/camera_worker.cpp
)

target_include_directories(rpi_cam PRIVATE include)
target_link_libraries(rpi_cam PRIVATE ${OpenCV_LIBS} mosquitto pthread)
