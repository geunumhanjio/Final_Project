#!/usr/bin/env python3
"""
scan_relay_node.py
==================
RPi ↔ WSL 시계 불일치 해결용 릴레이 노드.

/scan_raw (RPi, BEST_EFFORT) 구독
  → header.stamp을 WSL 현재 시각으로 교체
  → /scan (WSL, RELIABLE) 재발행

QoS 불일치 문제도 동시에 해결됨.
"""

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy


class ScanRelayNode(Node):
    def __init__(self):
        super().__init__('scan_relay')

        # RPi ydlidar 드라이버 기본 QoS: BEST_EFFORT
        sub_qos = QoSProfile(
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
        )

        self.sub = self.create_subscription(
            LaserScan,
            '/scan_raw',
            self._callback,
            sub_qos,
        )

        # SLAM / rviz2 기본 QoS: RELIABLE
        self.pub = self.create_publisher(LaserScan, '/scan', 10)

        self.get_logger().info('Scan relay ready: /scan_raw (BEST_EFFORT) → /scan (RELIABLE)')

    def _callback(self, msg: LaserScan) -> None:
        msg.header.stamp = self.get_clock().now().to_msg()
        self.pub.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = ScanRelayNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
