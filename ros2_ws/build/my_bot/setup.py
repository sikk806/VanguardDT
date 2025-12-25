import os
from glob import glob
from setuptools import find_packages, setup

package_name = 'my_bot'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        
        # --- [여기부터 추가된 부분] ---
        # 1. launch 폴더 안의 모든 .launch.py 파일을 share/패키지명/launch 로 복사
        (os.path.join('share', package_name, 'launch'), glob(os.path.join('launch', '*launch.py'))),
        
        # 2. urdf 폴더 안의 모든 .urdf 파일을 share/패키지명/urdf 로 복사
        (os.path.join('share', package_name, 'urdf'), glob(os.path.join('urdf', '*.urdf'))),
        # ---------------------------
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='ssafy',
    maintainer_email='ssafy@todo.todo',
    description='TODO: Package description',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'serial_bridge = my_bot.serial_bridge:main',
        ],
    }
)
