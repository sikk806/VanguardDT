#include "motor_bridge_node.h"
#include "utils.h"

#include <sstream>
#include <iomanip>
#include <cmath>

static int clamp_i(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static int to_gui_speed_0_100(int8_t speed_percent) {
    int s = (speed_percent >= 0) ? speed_percent : -speed_percent; // magnitude
    return clamp_i(s, 0, 100);
}

static int to_gui_steer_0_180(int8_t steer_percent) {
    // -100..100 -> 0..180 (90 = straight)
    int v = 90 + (int)std::lround((double)steer_percent * 90.0 / 100.0);
    return clamp_i(v, 0, 180);
}

MotorBridgeNode::MotorBridgeNode()
: rclcpp::Node("serial_bridge_node")
{
    // -----------------------------
    // Params
    // -----------------------------
    this->declare_parameter<std::string>("uart_device", "/dev/ttyACM0");
    this->declare_parameter<int>("uart_baud", 115200);

    this->declare_parameter<std::string>("mqtt_host", "127.0.0.1");
    this->declare_parameter<int>("mqtt_port", 1883);
    this->declare_parameter<std::string>("mqtt_topic", "robot/motor_cmd");

    this->declare_parameter<std::string>("cmd_vel_topic", "/cmd_vel");
    this->declare_parameter<std::string>("enable_topic", "");
    this->declare_parameter<std::string>("estop_topic",  "");

    this->declare_parameter<double>("speed_gain", 200.0);
    this->declare_parameter<double>("steer_gain", 500.0);
    this->declare_parameter<int>("send_rate_hz", 20);
    this->declare_parameter<int>("cmd_timeout_ms", 300);

    this->declare_parameter<bool>("start_stm_log", false);

    uart_device_    = this->get_parameter("uart_device").as_string();
    uart_baud_      = this->get_parameter("uart_baud").as_int();

    mqtt_host_      = this->get_parameter("mqtt_host").as_string();
    mqtt_port_      = this->get_parameter("mqtt_port").as_int();
    mqtt_topic_     = this->get_parameter("mqtt_topic").as_string();

    cmd_vel_topic_  = this->get_parameter("cmd_vel_topic").as_string();
    enable_topic_   = this->get_parameter("enable_topic").as_string();
    estop_topic_    = this->get_parameter("estop_topic").as_string();

    speed_gain_     = this->get_parameter("speed_gain").as_double();
    steer_gain_     = this->get_parameter("steer_gain").as_double();
    send_rate_hz_   = this->get_parameter("send_rate_hz").as_int();
    this->declare_parameter<int>("mqtt_rate_hz", 4);
    mqtt_rate_hz_ = this->get_parameter("mqtt_rate_hz").as_int();
    
    cmd_timeout_ms_ = this->get_parameter("cmd_timeout_ms").as_int();
    start_stm_log_  = this->get_parameter("start_stm_log").as_bool();

    // -----------------------------
    // UART open (실패하면 가상모드)
    // -----------------------------
    if (!rc_car::open(uart_device_, uart_baud_)) {
        uart_virtual_ = true;
        RCLCPP_WARN(get_logger(), "⚠️ UART 포트 오픈 실패 (%s). [가상 모드]로 동작합니다.",
                    uart_device_.c_str());
        RCLCPP_WARN(get_logger(), "👉 UART 전송 대신 화면에 [가상 전송] 로그만 출력합니다.");
    } else {
        RCLCPP_INFO(get_logger(), "✅ UART 연결 성공 (%s, %d)", uart_device_.c_str(), uart_baud_);
        if (start_stm_log_) {
            rc_car::start_log_thread();
            RCLCPP_INFO(get_logger(), "STM 로그 스레드 시작");
        }
    }

    // -----------------------------
    // MQTT connect (실패해도 계속 진행)
    // -----------------------------
    const std::string client_id = "ros_motor_bridge";
    if (!mqtt_.connect(mqtt_host_, mqtt_port_, client_id)) {
        RCLCPP_WARN(get_logger(), "⚠️ MQTT 연결 실패 (%s:%d). GUI publish는 실패할 수 있음.",
                    mqtt_host_.c_str(), mqtt_port_);
    } else {
        RCLCPP_INFO(get_logger(), "✅ MQTT 연결 성공 (%s:%d) topic=%s",
                    mqtt_host_.c_str(), mqtt_port_, mqtt_topic_.c_str());
    }

    // -----------------------------
    // Subscribe: /cmd_vel
    // -----------------------------
    sub_cmd_vel_ = this->create_subscription<geometry_msgs::msg::Twist>(
        cmd_vel_topic_, rclcpp::QoS(10),
        std::bind(&MotorBridgeNode::on_cmd_vel, this, std::placeholders::_1)
    );

    // optional enable / estop
    if (!enable_topic_.empty()) {
        sub_enable_ = this->create_subscription<std_msgs::msg::Bool>(
            enable_topic_, rclcpp::QoS(10),
            std::bind(&MotorBridgeNode::on_enable, this, std::placeholders::_1)
        );
    }
    if (!estop_topic_.empty()) {
        sub_estop_ = this->create_subscription<std_msgs::msg::Bool>(
            estop_topic_, rclcpp::QoS(10),
            std::bind(&MotorBridgeNode::on_estop, this, std::placeholders::_1)
        );
    }

    last_cmd_time_ = this->now();
    last_mqtt_pub_time_ = this->now();


    // -----------------------------
    // Timer loop: UART + MQTT
    // -----------------------------
    if (send_rate_hz_ < 1) send_rate_hz_ = 1;
    int period_ms = (int)std::lround(1000.0 / (double)send_rate_hz_);
    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(period_ms),
        std::bind(&MotorBridgeNode::on_timer, this)
    );

    RCLCPP_INFO(get_logger(),
        "Bridge start: sub=%s uart=%s(%s) mqtt=%s:%d topic=%s rate=%dHz timeout=%dms",
        cmd_vel_topic_.c_str(),
        uart_device_.c_str(),
        uart_virtual_ ? "VIRTUAL" : "REAL",
        mqtt_host_.c_str(), mqtt_port_, mqtt_topic_.c_str(),
        send_rate_hz_, cmd_timeout_ms_);
}

MotorBridgeNode::~MotorBridgeNode() {
    rc_car::stop_log_thread();
    rc_car::close();
    mqtt_.disconnect();
}

void MotorBridgeNode::on_cmd_vel(const geometry_msgs::msg::Twist::SharedPtr msg)
{
    last_twist_ = *msg;
    last_cmd_time_ = this->now();
}

void MotorBridgeNode::on_enable(const std_msgs::msg::Bool::SharedPtr msg)
{
    enable_ = msg->data;
}

void MotorBridgeNode::on_estop(const std_msgs::msg::Bool::SharedPtr msg)
{
    estop_ = msg->data;
}

std::string MotorBridgeNode::build_motor_cmd_json(const rc_car::DriveCmd& cmd, bool enable, bool estop) const
{
    const int gui_speed = to_gui_speed_0_100(cmd.speed);
    const int gui_steer = to_gui_steer_0_180(cmd.steer);

    std::ostringstream oss;
    oss << "{";
    oss << "\"source\":\"ros_cmd_vel\",";
    oss << "\"mode\":\"auto\",";
    oss << "\"speed\":" << gui_speed << ",";
    oss << "\"steer\":" << gui_steer << ",";
    oss << "\"enable\":" << (enable ? "true" : "false") << ",";
    oss << "\"emergency_stop\":" << (estop ? "true" : "false") << ",";
    oss << "\"raw\":{"
        << "\"speed_byte\":" << (int)cmd.speed << ","
        << "\"steer_byte\":" << (int)cmd.steer << ","
        << "\"flags_byte\":" << (int)cmd.flags
        << "},";
    oss << "\"timestamp\":\"" << utils::iso8601_utc_now() << "\"";
    oss << "}";
    return oss.str();
}

void MotorBridgeNode::log_virtual_uart(double v, double w, const rc_car::DriveCmd& cmd)
{
    // 너가 python에서 찍던 스타일 유지: S,v,w,E
    char ascii[128];
    std::snprintf(ascii, sizeof(ascii), "S,%.2f,%.2f,E", v, w);

    // 실제 패킷도 “이렇게 나갈 것”까지 보여주기 (AA 55 seq speed steer flags checksum)
    uint8_t pkt[7];
    pkt[0] = 0xAA;
    pkt[1] = 0x55;
    pkt[2] = virtual_seq_++;
    pkt[3] = (uint8_t)cmd.speed;
    pkt[4] = (uint8_t)cmd.steer;
    pkt[5] = (uint8_t)cmd.flags;
    uint8_t cs = 0;
    for (int i = 0; i < 6; ++i) cs += pkt[i];
    pkt[6] = cs;

    std::ostringstream hex;
    hex << std::uppercase << std::hex << std::setfill('0');
    for (int i = 0; i < 7; ++i) {
        hex << std::setw(2) << (int)pkt[i];
        if (i != 6) hex << " ";
    }

    RCLCPP_INFO(get_logger(), "[가상 전송] STM32로 갈 데이터: %s | PKT: %s",
                ascii, hex.str().c_str());
}

void MotorBridgeNode::on_timer()
{
    // cmd timeout이면 강제로 0으로
    const auto now = this->now();
    const int64_t dt_ms = (now - last_cmd_time_).nanoseconds() / 1000000;

    double v = last_twist_.linear.x;   // m/s
    double w = last_twist_.angular.z;  // rad/s

    if (dt_ms > cmd_timeout_ms_) {
        v = 0.0;
        w = 0.0;
    }

    // v,w -> -100..100
    int speed_p = (int)std::lround(v * speed_gain_);
    int steer_p = (int)std::lround(w * steer_gain_);
    speed_p = clamp_i(speed_p, -100, 100);
    steer_p = clamp_i(steer_p, -100, 100);

    rc_car::DriveCmd cmd{};
    cmd.speed = 40;
    cmd.steer = (int8_t)steer_p;

    uint8_t flags = 0;
    if (enable_) flags |= rc_car::FLAG_ENABLE;
    if (estop_)  flags |= rc_car::FLAG_ESTOP;
    cmd.flags = flags;

    // estop은 안전상 무조건 정지
    if (estop_) {
        cmd.speed = 0;
        cmd.steer = 0;
    }

    // 1) UART 전송 (없으면 가상 출력)
    if (!uart_virtual_ && rc_car::is_open()) {
        if (!rc_car::send(cmd)) {
            RCLCPP_WARN(get_logger(), "UART send failed -> fallback to virtual mode");
            uart_virtual_ = true;
            log_virtual_uart(v, w, cmd);
        }
    } else {
        log_virtual_uart(v, w, cmd);
    }

    // 2) MQTT publish (GUI) 레이트 리밋
    bool force_pub = false;

    // 처음 1회는 무조건
    if (!mqtt_inited_) force_pub = true;

    // enable/estop 토글은 즉시 GUI에 보여주기
    if (enable_ != last_mqtt_enable_ || estop_ != last_mqtt_estop_)
        force_pub = true;

    double min_period_sec = 0.0;
    if (mqtt_rate_hz_ > 0)
        min_period_sec = 1.0 / (double)mqtt_rate_hz_;

    const auto now2 = this->now();
    const double dt = (now2 - last_mqtt_pub_time_).seconds();

        if (force_pub || (mqtt_rate_hz_ <= 0) || (dt >= min_period_sec))
    {
        rc_car::DriveCmd cmd_mqtt = cmd;

        if (!estop_) {
            cmd_mqtt.speed = static_cast<int8_t>(40);
        } else {
            cmd_mqtt.speed = 0;
            cmd_mqtt.steer = 0;
        }

        const std::string payload = build_motor_cmd_json(cmd_mqtt, enable_, estop_);
        mqtt_.publish(mqtt_topic_, payload);

        last_mqtt_pub_time_ = now2;
        mqtt_inited_ = true;
        last_mqtt_enable_ = enable_;
        last_mqtt_estop_ = estop_;
    }
}
