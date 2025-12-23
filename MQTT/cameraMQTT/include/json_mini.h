#pragma once
#include <optional>
#include <string>

namespace json_mini {
    std::optional<std::string> getString(const std::string& s, const std::string& key);
    std::optional<int> getInt(const std::string& s, const std::string& key);
}
