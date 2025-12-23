#pragma once
#include <cstdint>

namespace rc_car {

// 기존 그대로: -100~100, flags(bit0 enable, bit1 estop)
struct DriveCmd {
    int8_t  speed;  // -100 ~ 100
    int8_t  steer;  // -100 ~ 100
    uint8_t flags;  // bit flags (bit0: enable, bit1: estop)
};

enum : uint8_t {
    FLAG_ENABLE = 1 << 0,
    FLAG_ESTOP  = 1 << 1,
};

} // namespace rc_car
