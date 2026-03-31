#!/usr/bin/env python3
"""
servo_tilt_node.py
==================
카메라 틸트 서보 모터 제어 노드 (Raspberry Pi 전용)

하드웨어:
  - GPIO 핀: 기본 GPIO 18 (pigpio hardware PWM 지원 핀)
  - 서보 PWM: 50Hz, 펄스폭 500~2500µs (180° 서보 기준)
  - 0°: 500µs / 90°: 1500µs / 180°: 2500µs

의존성:
  - pigpio: sudo apt install pigpio python3-pigpio
  - pigpiod 데몬 실행 필요: sudo systemctl enable pigpiod && sudo systemctl start pigpiod

구독:
  /camera/tilt  (std_msgs/Float32)  각도 명령 (deg, 범위: tilt_min ~ tilt_max, 절대각 0~180°)

발행:
  /camera/tilt_status  (std_msgs/Float32)  현재 서보 각도
"""

import rclpy
from rclpy.node import Node
from std_msgs.msg import Float32

try:
    import pigpio
    PIGPIO_AVAILABLE = True
except ImportError:
    PIGPIO_AVAILABLE = False


# ── 서보 PWM 변환 상수 (180° 서보 기준) ────────────────────────────────────
MIN_PW      = 500    # µs (0°)
MAX_PW      = 2500   # µs (180°)
SERVO_FREQ  = 50     # Hz


def angle_to_pulsewidth(angle_deg: float) -> int:
    """절대각(0~180°) → 펄스폭(µs) 변환. 0°=500µs, 180°=2500µs."""
    return int(MIN_PW + (angle_deg / 180.0) * (MAX_PW - MIN_PW))


class ServoTiltNode(Node):

    def __init__(self):
        super().__init__('servo_tilt_node')

        # ── 파라미터 ──────────────────────────────────────────────────────
        self.declare_parameter('gpio_pin',  18)      # hardware PWM 핀
        self.declare_parameter('tilt_min',  15.0)    # 최소 각도 (deg)
        self.declare_parameter('tilt_max',  90.0)    # 최대 각도 (deg)
        self.declare_parameter('init_angle', 15.0)   # 시작 각도 (deg)

        self._pin       = self.get_parameter('gpio_pin').value
        self._tilt_min  = self.get_parameter('tilt_min').value
        self._tilt_max  = self.get_parameter('tilt_max').value
        self._cur_angle = self.get_parameter('init_angle').value

        # ── pigpio 초기화 ─────────────────────────────────────────────────
        self._pi = None
        if PIGPIO_AVAILABLE:
            self._pi = pigpio.pi()
            if not self._pi.connected:
                self.get_logger().error(
                    'pigpiod 데몬에 연결 실패! '
                    '"sudo systemctl start pigpiod" 후 재시작하세요.'
                )
                self._pi = None
            else:
                self._set_angle(self._cur_angle)
                self.get_logger().info(
                    f'pigpio 연결 완료. GPIO {self._pin}, '
                    f'범위 [{self._tilt_min}°, {self._tilt_max}°] (180° 서보)'
                )
        else:
            self.get_logger().warn(
                'pigpio 미설치 — 서보 출력 없이 각도만 추적합니다. '
                '"pip install pigpio" 로 설치하세요.'
            )

        # ── ROS 인터페이스 ────────────────────────────────────────────────
        self._sub = self.create_subscription(
            Float32, '/camera/tilt', self._tilt_callback, 10)

        self._status_pub = self.create_publisher(
            Float32, '/camera/tilt_status', 10)

        # 1Hz로 현재 각도 상태 발행
        self.create_timer(1.0, self._publish_status)

        self.get_logger().info('ServoTiltNode 시작')

    def _tilt_callback(self, msg: Float32):
        angle = float(msg.data)
        angle = max(self._tilt_min, min(self._tilt_max, angle))  # 클램핑
        self._set_angle(angle)

    def _set_angle(self, angle_deg: float):
        self._cur_angle = angle_deg
        if self._pi and self._pi.connected:
            pw = angle_to_pulsewidth(angle_deg)
            self._pi.set_servo_pulsewidth(self._pin, pw)
        self.get_logger().debug(f'서보 각도: {angle_deg:.1f}°')

    def _publish_status(self):
        msg = Float32()
        msg.data = float(self._cur_angle)
        self._status_pub.publish(msg)

    def destroy_node(self):
        # 종료 시 서보 중립 복귀 후 PWM 해제
        if self._pi and self._pi.connected:
            self._pi.set_servo_pulsewidth(self._pin, CENTER_PW)
            self._pi.set_servo_pulsewidth(self._pin, 0)  # PWM 출력 중단
            self._pi.stop()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = ServoTiltNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
