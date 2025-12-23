#include "utils.h"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace utils {

int64_t now_unix_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string iso8601_utc_now() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::time_t t = system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&t, &utc);

    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setw(3) << std::setfill('0') << ms.count() << "Z";
    return oss.str();
}

} // namespace utils
