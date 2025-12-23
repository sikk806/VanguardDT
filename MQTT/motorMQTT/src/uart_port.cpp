#include "uart_port.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include <thread>
#include <atomic>
#include <fstream>
#include <iostream>

#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>

namespace rc_car {

namespace {
    int g_fd = -1;
    uint8_t g_seq = 0;

    std::thread      g_log_thread;
    std::atomic_bool g_log_running{false};
    std::string      g_log_path = "stm_log.txt";

    speed_t to_speed_t(int baud) {
        switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200:
        default:     return B115200;
        }
    }

    bool setup_serial(int fd, int baud) {
        struct termios tty;
        std::memset(&tty, 0, sizeof(tty));

        if (tcgetattr(fd, &tty) != 0) {
            std::perror("tcgetattr");
            return false;
        }

        speed_t spd = to_speed_t(baud);
        cfsetospeed(&tty, spd);
        cfsetispeed(&tty, spd);

        // 8N1, no flow control
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
                         INLCR | PARMRK | INPCK | ISTRIP);

        tty.c_lflag = 0;   // raw
        tty.c_oflag = 0;

        tty.c_cc[VMIN]  = 0;
        tty.c_cc[VTIME] = 1; // 0.1s timeout

        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
            std::perror("tcsetattr");
            return false;
        }
        return true;
    }

    void ensure_logs_dir() {
        struct stat st{};
        if (stat("logs", &st) == 0) {
            if (!S_ISDIR(st.st_mode)) {
                std::cerr << "[rc_car] 'logs' exists but is not a directory\n";
            }
            return;
        }
        if (mkdir("logs", 0755) != 0) {
            if (errno != EEXIST) {
                std::perror("[rc_car] mkdir logs");
            }
        }
    }

    std::string make_log_path_from_now() {
        std::time_t t = std::time(nullptr);
        std::tm lt{};
        localtime_r(&t, &lt);

        char buf[32];
        if (std::strftime(buf, sizeof(buf), "%y%m%d_%H%M", &lt) == 0) {
            return "logs/unknown_log.txt";
        }
        return std::string("logs/") + buf + ".txt";
    }

    void log_thread_func() {
        std::ofstream log(g_log_path, std::ios::app);
        if (!log.is_open()) {
            std::cerr << "[rc_car] WARN: cannot open log file: " << g_log_path << "\n";
        } else {
            std::cerr << "[rc_car] logging to: " << g_log_path << "\n";
        }

        uint8_t buf[256];

        while (g_log_running.load()) {
            ssize_t n = ::read(g_fd, buf, sizeof(buf));

            if (n > 0) {
                // 터미널 출력은 꺼둠(성능)
                if (log.is_open()) {
                    log.write(reinterpret_cast<char*>(buf), n);
                }
            } else if (n == 0) {
                ::usleep(1000);
            } else {
                if (errno == EINTR) {
                    if (!g_log_running.load()) break;
                    continue;
                }
                std::perror("[rc_car] read");
                break;
            }
        }
    }
} // namespace

bool open(const std::string& dev, int baud) {
    if (g_fd != -1) {
        std::cerr << "[rc_car] already opened\n";
        return false;
    }

    int fd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::perror("[rc_car] open");
        return false;
    }

    if (!setup_serial(fd, baud)) {
        ::close(fd);
        return false;
    }

    g_fd = fd;
    std::cout << "[rc_car] Opened serial: " << dev << " (" << baud << " 8N1)\n";
    return true;
}

bool is_open() {
    return (g_fd >= 0);
}

void close() {
    stop_log_thread();
    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
}

bool send(const DriveCmd& cmd) {
    if (g_fd < 0) {
        std::cerr << "[rc_car] send() failed: serial not open\n";
        return false;
    }

    int8_t speed = cmd.speed;
    int8_t steer = cmd.steer;
    uint8_t flags = cmd.flags;

    if (speed > 100)  speed = 100;
    if (speed < -100) speed = -100;
    if (steer > 100)  steer = 100;
    if (steer < -100) steer = -100;

    uint8_t pkt[7];
    pkt[0] = 0xAA;
    pkt[1] = 0x55;
    pkt[2] = g_seq++; // seq
    pkt[3] = static_cast<int8_t>(speed);
    pkt[4] = static_cast<int8_t>(steer);
    pkt[5] = static_cast<uint8_t>(flags);

    uint8_t checksum = 0;
    for (int i = 0; i < 6; i++) checksum += pkt[i];
    pkt[6] = checksum;

    ssize_t w = ::write(g_fd, pkt, sizeof(pkt));
    if (w < 0) {
        std::perror("[rc_car] write");
        return false;
    }
    if (w != (ssize_t)sizeof(pkt)) {
        std::cerr << "[rc_car] partial write: " << w << "\n";
        return false;
    }
    return true;
}

bool start_log_thread() {
    if (g_fd < 0) {
        std::cerr << "[rc_car] start_log_thread() failed: serial not open\n";
        return false;
    }
    if (g_log_running.load()) return true;

    ensure_logs_dir();
    g_log_path = make_log_path_from_now();

    g_log_running.store(true);
    g_log_thread = std::thread(log_thread_func);
    return true;
}

void stop_log_thread() {
    if (!g_log_running.load()) return;
    g_log_running.store(false);
    if (g_log_thread.joinable()) g_log_thread.join();
}

} // namespace rc_car
