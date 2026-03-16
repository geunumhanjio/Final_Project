#!/usr/bin/env python3
"""
scan_relay_node.py
==================
RPi → WSL 릴레이 노드.

전제: chrony로 RPi ↔ WSL 시각 동기화 완료.

/scan_raw (RPi, BEST_EFFORT) 구독
  → scan_delay_ms 만큼 지연 후 RPi 원본 타임스탬프로 /scan 재발행

  [delay buffer 전략]
  scan을 바로 발행하면, 해당 RPi 타임스탬프의 odom이 아직 WiFi를 타고
  미도착 상태일 수 있어 TF 조회 오차 발생.
  scan_delay_ms(기본 100ms) 동안 버퍼링하면:
    - 그 사이 odom이 도착하여 relay_ekf가 TF를 발행 완료
    - SLAM이 scan stamp(T_rpi)로 TF 조회 시 정확한 값 반환
    - WiFi jitter(최대 ~80ms) 흡수

/odom (RPi EKF 출력, RPi 시각) 구독
  → header.stamp을 WSL 시각으로 보정 후 /odom_relay 재발행
  → odom → base_footprint TF 발행

타임스탬프 전략:
  TF:        오는 즉시 WSL now로 발행 + keepalive(50Hz): WiFi 갭 TF 연속 유지
  scan:      RPi 원본 stamp 유지, scan_delay_ms 후 발행 (chrony 동기화 전제)
  odom_relay: RPi 델타 앵커 기반 WSL 시각 보정 (Nav2용)
"""

from collections import deque
import copy
import heapq

import rclpy
from rclpy.node import Node
from rclpy.time import Time
from sensor_msgs.msg import LaserScan
from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy
from tf2_ros import TransformBroadcaster


_KEEPALIVE_GAP_NS  = 40_000_000   # 40ms: 이 이상 odom 없으면 keepalive 발행
_MAX_RPI_DELTA_NS  = 500_000_000  # 500ms: 재시작·긴 정지 판정 임계값
_ANCHOR_HISTORY    = 60           # 최근 odom anchor 보관 수 (50Hz × 1.2s)


class ScanRelayNode(Node):
    def __init__(self):
        super().__init__('scan_relay')

        self.declare_parameter('use_ekf_relay', True)
        self.declare_parameter('publish_tf', True)
        self.declare_parameter('scan_delay_ms', 100)  # scan 지연 버퍼 (ms)
        self._use_ekf_relay  = self.get_parameter('use_ekf_relay').value
        self._publish_tf     = self.get_parameter('publish_tf').value
        self._scan_delay_ns  = int(self.get_parameter('scan_delay_ms').value * 1_000_000)

        sub_qos = QoSProfile(
            depth=10,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
        )

        self.sub = self.create_subscription(
            LaserScan, '/scan_raw', self._scan_callback, sub_qos)
        self.pub = self.create_publisher(LaserScan, '/scan', 10)

        # odom_relay용 앵커 히스토리
        self._anchor_history: deque = deque(maxlen=_ANCHOR_HISTORY)

        # keepalive용 상태
        self._last_tf: TransformStamped | None = None   # 마지막으로 발행한 TF
        self._last_odom_recv_ns: int = 0                # 마지막 odom 수신 시각

        # scan delay buffer: (release_wsl_ns, seq, msg)
        self._scan_buffer: list = []
        self._scan_seq: int = 0
        # 50Hz flush: scan_delay_ms 경과 후 순서대로 발행
        self.create_timer(0.02, self._scan_flush_callback)
        self.get_logger().info(f'scan delay buffer: {self.get_parameter("scan_delay_ms").value}ms')

        if self._use_ekf_relay:
            self.ekf_odom_sub = self.create_subscription(
                Odometry, '/odom', self._ekf_odom_callback, 10)
            self.odom_pub = self.create_publisher(Odometry, '/odom_relay', 10)

            if self._publish_tf:
                self.tf_broadcaster = TransformBroadcaster(self)
                # WiFi 갭에서 TF 유지: 50Hz keepalive
                self._keepalive_timer = self.create_timer(0.02, self._keepalive_callback)
                self.get_logger().info(
                    'EKF relay mode: /odom → /odom_relay + TF (keepalive 50Hz)')
            else:
                self.tf_broadcaster = None
                self.get_logger().info(
                    'EKF relay mode: /odom → /odom_relay (TF 발행 비활성화)')
        else:
            self.get_logger().info('EKF relay mode 비활성화 (WSL EKF 사용 중)')

    # ── odom_relay용 타임스탬프 보정 ──────────────────────────────────────────

    def _odom_to_wsl_stamp(self, rpi_stamp_msg) -> int:
        """RPi odom 타임스탬프를 WSL 시각으로 변환 (odom_relay 용)."""
        now_ns = self.get_clock().now().nanoseconds
        rpi_ns = Time.from_msg(rpi_stamp_msg).nanoseconds

        if not self._anchor_history:
            return now_ns

        best = None
        for anc_rpi, anc_wsl in self._anchor_history:
            if anc_rpi <= rpi_ns:
                if best is None or anc_rpi > best[0]:
                    best = (anc_rpi, anc_wsl)

        if best is None:
            best = min(self._anchor_history, key=lambda x: x[0])

        rpi_delta = rpi_ns - best[0]
        if rpi_delta >= _MAX_RPI_DELTA_NS:
            return now_ns

        return best[1] + rpi_delta

    # ── TF 발행 헬퍼 ─────────────────────────────────────────────────────────

    def _send_tf(self, stamp_ns: int) -> None:
        """self._last_tf의 pose를 stamp_ns 시각으로 발행."""
        if self._last_tf is None:
            return
        tf = copy.copy(self._last_tf)
        tf.header.stamp = Time(nanoseconds=stamp_ns).to_msg()
        self.tf_broadcaster.sendTransform(tf)

    # ── /odom 콜백 ────────────────────────────────────────────────────────────

    def _ekf_odom_callback(self, msg: Odometry) -> None:
        """RPi EKF /odom → /odom_relay 발행 + TF(now) 발행"""
        rpi_ns = Time.from_msg(msg.header.stamp).nanoseconds
        now_ns = self.get_clock().now().nanoseconds
        self._last_odom_recv_ns = now_ns

        # odom_relay 타임스탬프 보정 (앵커 델타)
        stamp_ns = self._odom_to_wsl_stamp(msg.header.stamp)
        self._anchor_history.append((rpi_ns, stamp_ns))

        odom_stamp_ns = min(stamp_ns, now_ns)   # Nav2: 미래 타임스탬프 거부
        msg.header.stamp = Time(nanoseconds=odom_stamp_ns).to_msg()
        self.odom_pub.publish(msg)

        # TF: WSL now 기준 발행 (keepalive와 동일 기준, 단순·일관성↑)
        if self._publish_tf:
            tf = TransformStamped()
            tf.header.frame_id = msg.header.frame_id    # 'odom'
            tf.child_frame_id = msg.child_frame_id      # 'base_footprint'
            tf.transform.translation.x = msg.pose.pose.position.x
            tf.transform.translation.y = msg.pose.pose.position.y
            tf.transform.translation.z = msg.pose.pose.position.z
            tf.transform.rotation = msg.pose.pose.orientation
            self._last_tf = tf
            self._send_tf(now_ns)

    # ── keepalive: WiFi 갭에서 TF 연속 유지 ──────────────────────────────────

    def _keepalive_callback(self) -> None:
        """
        마지막 odom 수신 후 _KEEPALIVE_GAP_NS 이상 경과 시
        마지막 위치로 TF를 현재 시각에 재발행.
        WiFi 갭(~200ms)에서 TF2 버퍼를 채워 scan TF 조회 실패 방지.
        """
        if self._last_tf is None:
            return
        now_ns = self.get_clock().now().nanoseconds
        if now_ns - self._last_odom_recv_ns < _KEEPALIVE_GAP_NS:
            return  # 최근 odom 있음: TF 이미 발행됨
        self._send_tf(now_ns)

    # ── /scan_raw 콜백 + delay buffer flush ──────────────────────────────────

    def _scan_callback(self, msg: LaserScan) -> None:
        """scan을 delay buffer에 적재. RPi 원본 stamp 유지."""
        now_ns = self.get_clock().now().nanoseconds
        release_ns = now_ns + self._scan_delay_ns
        heapq.heappush(self._scan_buffer, (release_ns, self._scan_seq, msg))
        self._scan_seq += 1

    def _scan_flush_callback(self) -> None:
        """release 시각이 된 scan을 순서대로 발행."""
        now_ns = self.get_clock().now().nanoseconds
        while self._scan_buffer and self._scan_buffer[0][0] <= now_ns:
            _, _, msg = heapq.heappop(self._scan_buffer)
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
