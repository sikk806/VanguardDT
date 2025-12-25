# 파일 경로: ~/ros2_ws/src/my_bot/my_bot/converter.py
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import struct

class CmdVelToSerial(Node):
    def __init__(self):
        super().__init__('cmd_vel_to_serial')

        # Nav2가 보내줄 속도 명령을 구독
        self.subscription = self.create_subscription(
            Twist,
            '/cmd_vel',
            self.listener_callback,
            10)

        # 로봇 바퀴 간격 (미터)
        self.WHEEL_DIST = 0.20 
        self.get_logger().info('UART Converter Node Started!')

    def listener_callback(self, msg):
        linear_x = msg.linear.x
        angular_z = msg.angular.z

        # 기구학: 좌우 바퀴 속도 계산 (m/s)
        left_speed = linear_x - (angular_z * self.WHEEL_DIST / 2)
        right_speed = linear_x + (angular_z * self.WHEEL_DIST / 2)

        # STM32로 보낼 값 변환 (예: x100 하고 정수로)
        pkt_left = int(left_speed * 100)
        pkt_right = int(right_speed * 100)

        # 패킷 생성 (헤더 0xFE + 왼쪽 2byte + 오른쪽 2byte)
        # 'h'는 short (2byte 정수)를 의미
        payload = struct.pack('hh', pkt_left, pkt_right)
        packet = b'\xFE' + payload

        # 로그 출력 (나중엔 ser.write(packet)으로 교체)
        self.get_logger().info(f'Send: {packet.hex().upper()}')

def main(args=None):
    rclpy.init(args=args)
    node = CmdVelToSerial()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
