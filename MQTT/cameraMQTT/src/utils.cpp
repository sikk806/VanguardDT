#include "utils.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>

namespace utils {
namespace fs = std::filesystem;

fs::path expandUser(const std::string& p) {
    if (!p.empty() && p[0] == '~') {
        const char* home = std::getenv("HOME");
        if (!home) home = "/home/ubuntu";
        if (p.size() == 1) return fs::path(home);
        if (p.size() >= 2 && p[1] == '/') return fs::path(home) / p.substr(2);
    }
    return fs::path(p);
}

void ensureDir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec) {
        std::cerr << "[ERR] mkdir failed: " << p << " : " << ec.message() << "\n";
    }
}

std::string nowIsoUtcZ() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    std::time_t t = system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&t, &tm);

    char buf[64];
    std::snprintf(buf, sizeof(buf),
                  "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

std::string getMyIpBestEffort() {
    FILE* fp = popen("hostname -I 2>/dev/null", "r");
    if (!fp) return "127.0.0.1";

    char buf[256]{0};
    std::fgets(buf, sizeof(buf), fp);
    pclose(fp);

    std::istringstream iss(buf);
    std::string ip;
    while (iss >> ip) {
        // 127.* 제외 / IPv6 제외
        if (ip.rfind("127.", 0) == 0) continue;
        if (ip.find(':') != std::string::npos) continue;
        return ip;
    }
    return "127.0.0.1";
}

} // namespace utils
