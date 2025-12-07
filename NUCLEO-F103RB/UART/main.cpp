#include <iostream>
#include "uart.hpp"

int main()
{
    const std::string dev = "/dev/ttyACM0";

    if (!rc_car::open(dev, 115200)) {
        std::cerr << "시리얼 오픈 실패\n";
        return 1;
    }

    // 내부에서 logs/YYMMDD_HHMM.txt 자동 생성
    rc_car::start_log_thread();

    std::cout << "입력 형식: speed steer flags\n";
    std::cout << "  speed: -100 ~ 100\n";
    std::cout << "  steer: -100 ~ 100\n";
    std::cout << "  flags: 0~255 (보통 1: enable, 3: enable+e-stop)\n";
    std::cout << "종료하려면 Ctrl+D 또는 Ctrl+C\n\n";

    rc_car::DriveCmd cmd;

    while (true) {
        std::cout << "패킷 입력 (speed steer flags) > ";
        std::cout.flush();

        int speed = 0, steer = 0, flags = 1;
        if (!(std::cin >> speed >> steer >> flags)) {
            std::cout << "\n입력 종료, 프로그램을 종료합니다.\n";
            break;
        }

        cmd.speed = static_cast<int8_t>(speed);
        cmd.steer = static_cast<int8_t>(steer);
        cmd.flags = static_cast<uint8_t>(flags);

        if (!rc_car::send(cmd)) {
            std::cerr << "전송 실패\n";
        }
    }

    rc_car::close();
    return 0;
}
