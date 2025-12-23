#include <iostream>
#include <thread>
#include <chrono>
#include "uart_port.h"

static void send_for(rc_car::DriveCmd& cmd,
                     int duration_ms, int speed, int steer, int flags = rc_car::FLAG_ENABLE)
{
    cmd.speed = (int8_t)speed;
    cmd.steer = (int8_t)steer;
    cmd.flags = (uint8_t)flags;

    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (!rc_car::send(cmd)) {
            std::cerr << "전송 실패\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed >= duration_ms) break;
    }
}

int main()
{
    const std::string dev = "/dev/ttyACM0";

    if (!rc_car::open(dev, 115200)) {
        std::cerr << "시리얼 오픈 실패\n";
        return 1;
    }

    rc_car::start_log_thread();

    rc_car::DriveCmd cmd{};
    std::cout << "RC카 자동 주행 시퀀스를 시작합니다.\n";

    send_for(cmd, 2000, 0, 0);
    send_for(cmd, 3000, 25, 0);
    send_for(cmd, 5000, 20, -50);
    send_for(cmd, 5000, 25, 0);
    send_for(cmd, 5000, 20, 50);
    send_for(cmd, 4000, -15, 0);
    send_for(cmd, 3000, 0, 0);

    cmd.speed = 0; cmd.steer = 0; cmd.flags = 0;
    rc_car::send(cmd);

    std::cout << "종료\n";
    rc_car::close();
    return 0;
}
