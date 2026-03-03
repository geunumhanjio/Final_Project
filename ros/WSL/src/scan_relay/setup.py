from setuptools import setup

package_name = 'scan_relay'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='doyun',
    maintainer_email='todo@todo.com',
    description='Scan relay node: re-timestamps /scan_raw from RPi to /scan on WSL',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'scan_relay = scan_relay.scan_relay_node:main',
        ],
    },
)
