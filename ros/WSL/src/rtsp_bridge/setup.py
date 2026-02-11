from setuptools import setup
import os
from glob import glob

package_name = 'rtsp_bridge'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # Launch 파일 추가
        (os.path.join('share', package_name, 'launch'), 
            glob('launch/*.launch.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='doyun',
    maintainer_email='your@email.com',
    description='RTSP bridge for ROS2',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'rtsp_publisher = rtsp_bridge.rtsp_publisher:main',
        ],
    },
)
