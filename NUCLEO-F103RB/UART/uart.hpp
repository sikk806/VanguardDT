// rc_car_uart.h
#pragma once

#include <cstdint>
#include <string>

namespace rc_car {

struct DriveCmd {
    int8_t  speed;  // -100 ~ 100
    int8_t  steer;  // -100 ~ 100
    uint8_t flags;  // bit flags (bit0: enable, bit1: estop)
};

bool open(const std::string& dev, int baud = 115200);
bool is_open();
void close();
bool send(const DriveCmd& cmd);

/// STM32 로그를 별도 쓰레드에서 읽어와서
/// - 터미널에 출력하고
/// - logs/YYMMDD_HHMM.txt 파일에 기록
/// 하는 기능을 켠다.
bool start_log_thread();

void stop_log_thread();

} // namespace rc_car
