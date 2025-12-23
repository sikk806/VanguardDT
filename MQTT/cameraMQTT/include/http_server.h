#pragma once
#include <atomic>
#include <filesystem>

class HttpServer {
public:
    HttpServer(std::filesystem::path dir, int port);
    ~HttpServer();

    bool start();
    void stop();

private:
    std::filesystem::path dir_;
    int port_;
    std::atomic<pid_t> pid_{-1};
};
