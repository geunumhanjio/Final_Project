#!/usr/bin/env python3
"""
scan_relay_node.py
==================
RPi ↔ WSL 시계 불일치 해결용 릴레이 노드.

/scan_raw (RPi, BEST_EFFORT) 구독
  → header.stamp을 WSL 현재 시각 기준으로 교체
  → /scan (WSL, RELIABLE) 재발행

/odom (RPi EKF 출력, RPi 시각) 구독
  → header.stamp을 WSL 현재 시각 기준으로 교체
  → /odom (WSL) 재발행
  → odom → base_footprint TF 발행 (WSL 시각 기준)

클럭 오프셋 추정:
  /wheel_odom (50Hz) 수신 시각과 RPi 스탬프의 차이로
  scan(8Hz)보다 빠르게 clock_offset + transmission_delay를 EMA 추적.
  → scan/odom 타임스탬프 = rpi_stamp + offset_ema - 전송지연 [- TF안전마진]

주의: EKF가 RPi에서 실행될 때만 /odom 릴레이 활성화.
  WSL EKF 사용 시에는 use_ekf_relay:=false로 비활성화.
"""

import rclpy
from rclpy.node import Node
from rclpy.time import Time
from sensor_msgs.msg import LaserScan
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy
from tf2_ros import TransformBroadcaster


_TRANSMISSION_EST_NS = 15_000_000   # 15ms: 예상 평균 전송 지연
_SAFETY_BUFFER_NS    = 80_000_000   # 80ms: EKF TF 발행 안전 마진
_FALLBACK_OFFSET_NS  = 30_000_000   # 30ms: EMA 수렴 전 폴백
_EMA_ALPHA           = 0.02         # odom 50Hz → ~50샘플(1초)에 수렴
_OFFSET_MIN_NS       = -int(365 * 24 * 3600 * 1_000_000_000)
_OFFSET_MAX_NS       =  int(365 * 24 * 3600 * 1_000_000_000)


class ScanRelayNode(Node):
    def __init__(self):
        super().__init__('scan_relay')

        self.declare_parameter('use_ekf_relay', True)
        self._use_ekf_relay = self.get_parameter('use_ekf_relay').value

        sub_qos = QoSProfile(
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
        )

        self.sub = self.create_subscription(
            LaserScan, '/scan_raw', self._scan_callback, sub_qos)

        # wheel_odom: 50Hz로 클럭 오프셋 빠르게 추적 (재발행 없음)
        self.odom_sub = self.create_subscription(
            Odometry, '/wheel_odom', self._wheel_odom_callback, 10)

        self.pub = self.create_publisher(LaserScan, '/scan', 10)

        if self._use_ekf_relay:
            # RPi EKF 출력(/odom) → 타임스탬프 보정 → WSL /odom 재발행 + TF
            self.ekf_odom_sub = self.create_subscription(
                Odometry, '/odom', self._ekf_odom_callback, 10)
            self.odom_pub = self.create_publisher(Odometry, '/odom_relay', 10)
            self.tf_broadcaster = TransformBroadcaster(self)
            self.get_logger().info('EKF relay mode: /odom → timestamp 보정 → TF 발행')
        else:
            self.get_logger().info('EKF relay mode 비활성화 (WSL EKF 사용 중)')

        self._offset_ema_ns: float | None = None
        self._sample_count = 0

    def _update_offset(self, rpi_stamp_msg) -> None:
        now_ns = self.get_clock().now().nanoseconds
        rpi_ns = Time.from_msg(rpi_stamp_msg).nanoseconds
        raw = now_ns - rpi_ns

        if not (_OFFSET_MIN_NS < raw < _OFFSET_MAX_NS):
            return

        self._sample_count += 1
        if self._offset_ema_ns is None:
            self._offset_ema_ns = float(raw)
            self.get_logger().info(
                f'Clock offset 초기 추정: {raw / 1e9:.3f}s'
            )
        else:
            self._offset_ema_ns = (
                _EMA_ALPHA * raw + (1.0 - _EMA_ALPHA) * self._offset_ema_ns
            )

        if self._sample_count % 500 == 0:
            self.get_logger().info(
                f'Clock offset EMA: {self._offset_ema_ns / 1e9:.4f}s '
                f'(samples={self._sample_count})'
            )

    def _correct_stamp(self, rpi_stamp_msg, safety_buffer: bool = False) -> int:
        now_ns = self.get_clock().now().nanoseconds
        rpi_ns = Time.from_msg(rpi_stamp_msg).nanoseconds

        if self._offset_ema_ns is not None:
            stamp_ns = rpi_ns + int(self._offset_ema_ns) - _TRANSMISSION_EST_NS
            if safety_buffer:
                stamp_ns -= _SAFETY_BUFFER_NS
                stamp_ns = min(stamp_ns, now_ns - _SAFETY_BUFFER_NS)
            else:
                stamp_ns = min(stamp_ns, now_ns)
        else:
            stamp_ns = now_ns - _FALLBACK_OFFSET_NS

        return int(stamp_ns)

    def _wheel_odom_callback(self, msg: Odometry) -> None:
        """50Hz wheel_odom → 클럭 오프셋 EMA 추적 전용"""
        self._update_offset(msg.header.stamp)

    def _ekf_odom_callback(self, msg: Odometry) -> None:
        """RPi EKF 출력 /odom → WSL 시각으로 보정 후 재발행 + TF 발행"""
        stamp_ns = self._correct_stamp(msg.header.stamp, safety_buffer=False)
        stamp_msg = Time(nanoseconds=stamp_ns).to_msg()

        # 보정된 /odom 재발행
        msg.header.stamp = stamp_msg
        self.odom_pub.publish(msg)

        # odom → base_footprint TF 발행
        tf = TransformStamped()
        tf.header.stamp = stamp_msg
        tf.header.frame_id = msg.header.frame_id        # 'odom'
        tf.child_frame_id = msg.child_frame_id          # 'base_footprint'
        tf.transform.translation.x = msg.pose.pose.position.x
        tf.transform.translation.y = msg.pose.pose.position.y
        tf.transform.translation.z = msg.pose.pose.position.z
        tf.transform.rotation = msg.pose.pose.orientation
        self.tf_broadcaster.sendTransform(tf)

    def _scan_callback(self, msg: LaserScan) -> None:
        stamp_ns = self._correct_stamp(msg.header.stamp, safety_buffer=True)
        msg.header.stamp = Time(nanoseconds=stamp_ns).to_msg()
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
