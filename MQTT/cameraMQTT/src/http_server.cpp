#include "http_server.h"
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

HttpServer::HttpServer(std::filesystem::path dir, int port)
    : dir_(std::move(dir)), port_(port) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::stop() {
    pid_t p = pid_.load();
    if (p > 0) {
        kill(p, SIGTERM);
        int st = 0;
        waitpid(p, &st, 0);
        pid_.store(-1);
        std::cerr << "[HTTP] stopped pid=" << p << "\n";
    }
}

bool HttpServer::start() {
    stop();

    pid_t p = fork();
    if (p < 0) {
        std::cerr << "[ERR] fork failed for http server\n";
        return false;
    }

    if (p == 0) {
        if (chdir(dir_.c_str()) != 0) _exit(127);

        // python3 -m http.server <port> --bind 0.0.0.0
        execlp("python3", "python3", "-m", "http.server",
               std::to_string(port_).c_str(),
               "--bind", "0.0.0.0",
               (char*)nullptr);
        _exit(127);
    }

    pid_.store(p);
    std::cerr << "[HTTP] serving " << dir_ << " on :" << port_ << " pid=" << p << "\n";
    return true;
}
