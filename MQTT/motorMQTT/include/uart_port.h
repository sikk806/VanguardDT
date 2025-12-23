#pragma once
#include <string>
#include "uart_packet.h"

namespace rc_car {

bool open(const std::string& dev, int baud = 115200);
bool is_open();
void close();
bool send(const DriveCmd& cmd);

// STM32에서 올라오는 로그를 파일에 저장(원하면 켜기)
bool start_log_thread();
void stop_log_thread();

} // namespace rc_car
