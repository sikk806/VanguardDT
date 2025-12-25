import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # URDF 파일 경로
    # (주의: 지금은 패키지 설치 경로 대신 소스 경로를 직접 지정해 테스트합니다)
    urdf_file = os.path.expanduser('~/ros2_ws/src/my_bot/urdf/my_bot.urdf')

    with open(urdf_file, 'r') as inf:
        robot_desc = inf.read()

    return LaunchDescription([
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{'robot_description': robot_desc}],
        )
    ])
