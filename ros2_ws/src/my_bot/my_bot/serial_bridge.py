import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import serial
import sys

class SerialBridge(Node):
    def __init__(self):
        super().__init__('serial_bridge_node')

        # 설정 변수
        self.port_name = '/dev/ttyACM0'
        self.baud_rate = 115200

        # 1. 시리얼 포트 연결 시도
        try:
            self.ser = serial.Serial(self.port_name, self.baud_rate, timeout=1)
            self.get_logger().info(f'✅ 시리얼 연결 성공! ({self.port_name})')
        except serial.SerialException:
            # 연결 실패 시 프로그램을 끄지 않고 '가상 모드'로 전환
            self.ser = None
            self.get_logger().warn(f'⚠️ 포트를 찾을 수 없습니다 ({self.port_name}).')
            self.get_logger().warn('👉 [가상 모드]로 동작합니다. 데이터는 화면에 출력됩니다.')

        # 2. /cmd_vel 토픽 구독 (Subscribe)
        self.subscription = self.create_subscription(
            Twist,
            '/cmd_vel',
            self.cmd_vel_callback,
            10
        )

    def cmd_vel_callback(self, msg):
        # 3. ROS 메시지에서 속도 추출
        linear_x = msg.linear.x   # 전진 속도 (m/s)
        angular_z = msg.angular.z # 회전 속도 (rad/s)

        # 4. STM32로 보낼 패킷 생성 (포맷: "S,선속도,각속도,E\n")
        # 예: S,0.22,-0.50,E
        packet = f"S,{linear_x:.2f},{angular_z:.2f},E\n"

        # 5. 전송 또는 출력
        if self.ser and self.ser.is_open:
            # [실제 모드] USB 선이 연결되어 있을 때
            try:
                self.ser.write(packet.encode('utf-8'))
                # 실제 전송 중에는 로그가 너무 빠르니 주석 처리하거나 필요할 때만 켬
                # self.get_logger().info(f'Sent: {packet.strip()}') 
            except Exception as e:
                self.get_logger().error(f'전송 중 에러 발생: {e}')
        else:
            # [가상 모드] 선이 없을 때 눈으로 확인
            self.get_logger().info(f'[가상 전송] STM32로 갈 데이터: {packet.strip()}')

def main(args=None):
    rclpy.init(args=args)
    node = SerialBridge()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        # 종료 시 시리얼 포트 닫기
        if node.ser and node.ser.is_open:
            node.ser.close()
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
