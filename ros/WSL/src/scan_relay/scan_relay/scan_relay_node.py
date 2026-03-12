#!/usr/bin/env python3
"""
scan_relay_node.py
==================
RPi ↔ WSL 시계 불일치 해결용 릴레이 노드.

/scan_raw (RPi, BEST_EFFORT) 구독
  → header.stamp을 WSL 현재 시각 기준으로 교체
  → /scan (WSL, RELIABLE) 재발행

클럭 오프셋 추정:
  /wheel_odom (50Hz) 수신 시각과 RPi 스탬프의 차이로
  scan(10Hz)보다 5배 빠르게 clock_offset + transmission_delay를 EMA 추적.
  → scan 타임스탬프 = rpi_scan_stamp + offset_ema - 전송지연 - TF안전마진

주의: odom/IMU는 보정하지 않음.
  EKF가 odom(RPi 시각) + IMU(RPi 시각)를 동일한 시간 기준으로 융합하므로
  내부 dt 계산이 정확함. odom만 보정하면 IMU와 시간 도메인이 달라져 오히려 오작동.
"""

import rclpy
from rclpy.node import Node
from rclpy.time import Time
from sensor_msgs.msg import LaserScan
from nav_msgs.msg import Odometry
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy


_TRANSMISSION_EST_NS = 15_000_000   # 15ms: 예상 평균 전송 지연
_SAFETY_BUFFER_NS    = 80_000_000   # 80ms: EKF TF 발행 안전 마진 (실측 TF 지연 ~56ms 기준)
_FALLBACK_OFFSET_NS  = 30_000_000   # 30ms: EMA 수렴 전 폴백
_EMA_ALPHA           = 0.02         # odom 50Hz → ~50샘플(1초)에 수렴
# Pi-WSL 클럭 오프셋 유효 범위
# Pi에 RTC 배터리 없으면 클럭이 수년 단위로 차이날 수 있음
# 60s 제한 시 모든 샘플이 rejected → EMA 수렴 불가 → scan이 로봇에 붙어서 이동하는 현상
_OFFSET_MIN_NS       = -int(365 * 24 * 3600 * 1_000_000_000)  # -1년 (Pi가 WSL보다 앞선 경우)
_OFFSET_MAX_NS       =  int(365 * 24 * 3600 * 1_000_000_000)  # +1년 (Pi가 WSL보다 뒤처진 경우)


class ScanRelayNode(Node):
    def __init__(self):
        super().__init__('scan_relay')

        sub_qos = QoSProfile(
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
        )

        self.sub = self.create_subscription(
            LaserScan, '/scan_raw', self._scan_callback, sub_qos)

        # wheel_odom 구독: 50Hz로 클럭 오프셋 빠르게 추적 (재발행은 하지 않음)
        self.odom_sub = self.create_subscription(
            Odometry, '/wheel_odom', self._odom_callback, 10)

        self.pub = self.create_publisher(LaserScan, '/scan', 10)

        # EMA: (WSL수신시각 - RPi스탬프) = clock_offset + transmission_delay
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
                f'Clock offset 초기 추정: {raw / 1e9:.3f}s '
                f'(transmission+offset 합산)'
            )
        else:
            self._offset_ema_ns = (
                _EMA_ALPHA * raw + (1.0 - _EMA_ALPHA) * self._offset_ema_ns
            )

        # 500샘플마다 현재 추정값 로그
        if self._sample_count % 500 == 0:
            self.get_logger().info(
                f'Clock offset EMA: {self._offset_ema_ns / 1e9:.4f}s '
                f'(samples={self._sample_count})'
            )

    def _odom_callback(self, msg: Odometry) -> None:
        """50Hz odom → 클럭 오프셋 EMA 추적 전용 (재발행 없음)"""
        self._update_offset(msg.header.stamp)

    def _scan_callback(self, msg: LaserScan) -> None:
        now_ns = self.get_clock().now().nanoseconds
        rpi_ns = Time.from_msg(msg.header.stamp).nanoseconds

        if self._offset_ema_ns is not None:
            # scan 수집 시각(WSL 기준) = rpi_stamp + clock_offset
            # ≈ rpi_stamp + offset_ema - transmission_est
            stamp_ns = (
                rpi_ns
                + int(self._offset_ema_ns)
                - _TRANSMISSION_EST_NS
                - _SAFETY_BUFFER_NS
            )
            stamp_ns = min(stamp_ns, now_ns - _SAFETY_BUFFER_NS)
        else:
            stamp_ns = now_ns - _FALLBACK_OFFSET_NS

        msg.header.stamp = Time(nanoseconds=int(stamp_ns)).to_msg()
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
