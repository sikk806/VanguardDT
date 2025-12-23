#pragma once
#include <string>
#include <filesystem>

struct AppConfig {
    std::string mqtt_host = "127.0.0.1";
    int mqtt_port = 1883;
    std::string client_id = "rpi_cam_service";

    int http_port = 8000;
    std::filesystem::path img_dir = "~/camera/camera_images";

    int cam_device_index = 0;
    int cam_width = 640;
    int cam_height = 480;

    int default_period_ms = 1000; // camera_cmd start에 period_ms 없으면 이 값
};
