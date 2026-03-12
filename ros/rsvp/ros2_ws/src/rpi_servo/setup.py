from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'rpi_servo'

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
    description='Camera tilt servo control node for Raspberry Pi',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'servo_tilt_node = rpi_servo.servo_tilt_node:main',
        ],
    },
)
