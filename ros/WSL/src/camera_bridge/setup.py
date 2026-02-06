from setuptools import setup
import os
from glob import glob

package_name = 'camera_bridge'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*.yaml')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='umdoyuun',
    maintainer_email='tkdtlr1998@gmail.com',
    description='Camera MJPEG bridge',
    license='Apache License 2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'mjpeg_bridge = camera_bridge.mjpeg_bridge:main',
        ],
    },
)
