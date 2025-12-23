#pragma once
#include <filesystem>
#include <string>

namespace utils {
    namespace fs = std::filesystem;

    fs::path expandUser(const std::string& p);
    void ensureDir(const fs::path& p);

    std::string nowIsoUtcZ();
    std::string getMyIpBestEffort();
}
