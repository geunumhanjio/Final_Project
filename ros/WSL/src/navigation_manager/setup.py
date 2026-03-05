from setuptools import setup
import os
from glob import glob

package_name = 'navigation_manager'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'config'),
            glob('config/*.yaml')),
        (os.path.join('share', package_name, 'launch'),
            glob('launch/*.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='doyun',
    maintainer_email='todo@todo.com',
    description='Navigation mission manager: waypoint navigation and patrol mode',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'navigation_manager = navigation_manager.navigation_manager_node:main',
        ],
    },
)
