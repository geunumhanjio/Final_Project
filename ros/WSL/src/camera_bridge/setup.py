# camera_bridge/setup.py
from setuptools import setup
import os
from glob import glob

package_name = 'camera_bridge'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # launch 파일 추가
        (os.path.join('share', package_name, 'launch'), 
            glob('launch/*.launch.py')),
        # config 파일 추가
        (os.path.join('share', package_name, 'config'), 
            glob('config/*.yaml')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='your_name',
    maintainer_email='your_email@example.com',
    description='Camera bridge package',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'mjpeg_bridge = camera_bridge.mjpeg_bridge:main',
        ],
    },
)
