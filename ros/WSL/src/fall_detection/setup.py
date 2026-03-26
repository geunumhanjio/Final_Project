from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'fall_detection'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Doyun',
    maintainer_email='your_email@example.com',
    description='MediaPipe Pose 기반 누워있는 상태(낙상) 감지 노드',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'fall_detection_node = fall_detection.fall_detection_node:main',
        ],
    },
)
