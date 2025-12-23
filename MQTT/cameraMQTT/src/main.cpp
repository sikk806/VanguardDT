#include "app.h"
#include <csignal>
#include <cstdlib>
#include <iostream>

static App* g_app = nullptr;

static void handleSig(int) {
    if (g_app) g_app->requestStop();
}

int main(int argc, char** argv) {
    AppConfig cfg;

    // args: <mqtt_host> <mqtt_port> <http_port>
    if (argc >= 2) cfg.mqtt_host = argv[1];
    if (argc >= 3) cfg.mqtt_port = std::atoi(argv[2]);
    if (argc >= 4) cfg.http_port = std::atoi(argv[3]);

    std::signal(SIGINT, handleSig);
    std::signal(SIGTERM, handleSig);

    App app(cfg);
    g_app = &app;

    return app.run();
}
