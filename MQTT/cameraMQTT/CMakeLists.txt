cmake_minimum_required(VERSION 3.16)
project(rpi_cam CXX) # package.xml의 <name>rpi_cam</name>과 일치해야 함

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. ROS 2 및 필수 패키지 찾기
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(OpenCV REQUIRED)

# 2. 실행 파일 추가
add_executable(rpi_cam
  src/main.cpp
  src/app.cpp
  src/utils.cpp
  src/http_server.cpp
  src/mqtt_client.cpp
  src/camera_worker.cpp
)

# 3. [중요] 헤더 경로 수정 (include 폴더가 없다면 src로 지정해야 함)
target_include_directories(rpi_cam PRIVATE include)

# 4. ROS 의존성 연결
ament_target_dependencies(rpi_cam
  rclcpp
  geometry_msgs
  nav_msgs
  tf2
  tf2_ros
  tf2_geometry_msgs
)

# 5. [핵심 수정] PRIVATE 키워드를 빼야 에러가 안 납니다!
target_link_libraries(rpi_cam
  ${OpenCV_LIBS}
  mosquitto
  pthread
)

# 6. 설치 규칙 (make install 시 필요)
install(TARGETS rpi_cam
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
