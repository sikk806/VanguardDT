import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import serial
import time

class SerialBridge(Node):
    def __init__(self):
        super().__init__('serial_bridge_node')
        
        # [설정] 포트와 보레이트
        self.port_name = '/dev/ttyUSB0'
        self.baudrate = 115200
        
        # [1] 시리얼 연결 시도
        try:
            self.ser = serial.Serial(self.port_name, self.baudrate, timeout=1)
            self.get_logger().info(f'✅ 진짜 하드웨어 연결 성공! ({self.port_name})')
            self.is_real_hardware = True
        except (serial.SerialException, FileNotFoundError):
            self.get_logger().warn('⚠️ STM32 연결 실패! -> [가상 시뮬레이션 모드]로 동작합니다.')
            self.is_real_hardware = False

        # [2] 로봇 제원 (내 로봇에 맞게 수정!)
        self.wheel_separation = 0.2
        self.wheel_radius = 0.033

        # [3] 구독자 생성
        self.subscription = self.create_subscription(
            Twist,
            'cmd_vel',
            self.cmd_vel_callback,
            10
        )

    def cmd_vel_callback(self, msg):
        linear_x = msg.linear.x
        angular_z = msg.angular.z

        # 역기구학
        left_speed_mps = linear_x - (angular_z * self.wheel_separation / 2)
        right_speed_mps = linear_x + (angular_z * self.wheel_separation / 2)

        # RPM 변환
        wheel_circumference = 2 * 3.14159 * self.wheel_radius
        left_rpm = (left_speed_mps / wheel_circumference) * 60
        right_rpm = (right_speed_mps / wheel_circumference) * 60

        cmd_l = int(left_rpm)
        cmd_r = int(right_rpm)

        # 패킷 생성
        packet = f"s,{cmd_l},{cmd_r},e\n"

        # 전송
        if self.is_real_hardware:
            self.ser.write(packet.encode('utf-8'))
        else:
            print(f"[가상전송] 생성된 패킷: {packet.strip()}")

def main(args=None):
    rclpy.init(args=args)     # <--- 여기서 rclpy를 쓰기 때문에 맨 위 import가 필수입니다.
    node = SerialBridge()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
