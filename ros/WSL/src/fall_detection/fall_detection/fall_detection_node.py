#!/usr/bin/env python3
"""
fall_detection_node.py
======================
MediaPipe Pose 기반 누워있는 상태(낙상) 감지 노드 (ROS2, CPU 전용)

동작:
  1. /camera/image_raw/compressed 구독
  2. MediaPipe Pose로 어깨→힙 벡터의 수직 기준 각도 계산
  3. 각도 > lying_angle_threshold (기본 45°)이면 누워있음 판정
  4. confirm_count 프레임 연속 판정 후 /fall_detection/alert 발행
  5. cooldown_sec (기본 20초) 쿨다운으로 중복 경고 억제
  6. /fall_detection/enable 로 ON/OFF 제어

구독:
  /camera/image_raw/compressed  (sensor_msgs/CompressedImage)  카메라 프레임
  /fall_detection/enable         (std_msgs/Bool)                감지 ON/OFF

발행:
  /fall_detection/alert   (std_msgs/String, JSON)  낙상 경고
  /fall_detection/status  (std_msgs/String, JSON)  1Hz 상태
"""

import json
import math
import time

import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage
from std_msgs.msg import Bool, String

try:
    import mediapipe as mp
    MP_AVAILABLE = True
except ImportError:
    MP_AVAILABLE = False

# MediaPipe Pose 랜드마크 인덱스
_L_SHOULDER = 11
_R_SHOULDER = 12
_L_HIP      = 23
_R_HIP      = 24

# 기본값 상수
_DEFAULT_MAX_FPS              = 5.0
_DEFAULT_LYING_ANGLE_THRESH   = 45.0   # deg (수직에서 벗어난 각도)
_DEFAULT_COOLDOWN_SEC         = 20.0   # 경고 재발행 최소 간격
_DEFAULT_CONFIRM_COUNT        = 3      # 연속 판정 횟수
_VIS_THRESHOLD                = 0.5    # 랜드마크 신뢰도 임계값


class FallDetectionNode(Node):

    def __init__(self):
        super().__init__('fall_detection_node')

        # ── 파라미터 ──────────────────────────────────────────────────────
        self.declare_parameter('max_fps',               _DEFAULT_MAX_FPS)
        self.declare_parameter('image_topic',           '/camera/image_raw/compressed')
        self.declare_parameter('lying_angle_threshold', _DEFAULT_LYING_ANGLE_THRESH)
        self.declare_parameter('cooldown_sec',          _DEFAULT_COOLDOWN_SEC)
        self.declare_parameter('confirm_count',         _DEFAULT_CONFIRM_COUNT)
        self.declare_parameter('enabled_on_start',      True)

        self._max_interval  = 1.0 / self.get_parameter('max_fps').value
        image_topic         = self.get_parameter('image_topic').value
        self._angle_thresh  = self.get_parameter('lying_angle_threshold').value
        self._cooldown_sec  = self.get_parameter('cooldown_sec').value
        self._confirm_limit = int(self.get_parameter('confirm_count').value)
        self._enabled       = self.get_parameter('enabled_on_start').value

        # ── MediaPipe Pose 초기화 ─────────────────────────────────────────
        self._pose = None
        if MP_AVAILABLE:
            self._pose = mp.solutions.pose.Pose(
                static_image_mode=False,
                model_complexity=0,        # 0=Lite, CPU 전용
                smooth_landmarks=True,
                min_detection_confidence=_VIS_THRESHOLD,
                min_tracking_confidence=_VIS_THRESHOLD,
            )
            self.get_logger().info('MediaPipe Pose 초기화 완료 (Lite 모드)')
        else:
            self.get_logger().error('mediapipe 미설치: pip install mediapipe')

        # ── 내부 상태 ─────────────────────────────────────────────────────
        self._last_infer_t = 0.0
        self._last_alert_t = 0.0   # 마지막 경고 발행 시각 (time.time 기준)
        self._confirm_cnt  = 0     # 연속 누워있음 판정 횟수

        # ── Publishers ────────────────────────────────────────────────────
        self._alert_pub  = self.create_publisher(String, '/fall_detection/alert',  10)
        self._status_pub = self.create_publisher(String, '/fall_detection/status', 10)

        # ── Subscribers ───────────────────────────────────────────────────
        self._img_sub = self.create_subscription(
            CompressedImage, image_topic, self._image_callback, 10)
        self._enable_sub = self.create_subscription(
            Bool, '/fall_detection/enable', self._enable_callback, 10)

        self.create_timer(1.0, self._publish_status)

        self.get_logger().info(
            f'FallDetectionNode 시작 (enabled={self._enabled}, '
            f'max_fps={self.get_parameter("max_fps").value}, '
            f'cooldown={self._cooldown_sec}s, '
            f'angle_thresh={self._angle_thresh}deg). '
            f'/fall_detection/enable 으로 ON/OFF 제어 가능.')

    # ── ON/OFF ────────────────────────────────────────────────────────────

    def _enable_callback(self, msg: Bool):
        self._enabled = msg.data
        if not self._enabled:
            self._confirm_cnt = 0
        self.get_logger().info(f'낙상 감지 {"활성화" if self._enabled else "비활성화"}')

    # ── 이미지 콜백 ───────────────────────────────────────────────────────

    def _image_callback(self, msg: CompressedImage):
        if not self._enabled or self._pose is None:
            return

        now = time.monotonic()
        if now - self._last_infer_t < self._max_interval:
            return
        self._last_infer_t = now

        np_arr = np.frombuffer(msg.data, np.uint8)
        frame  = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
        if frame is None:
            return

        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        try:
            results = self._pose.process(frame_rgb)
        except Exception as e:
            self.get_logger().warn(f'MediaPipe 추론 오류: {e}')
            return

        if results.pose_landmarks is None:
            self._confirm_cnt = 0
            return

        lm = results.pose_landmarks.landmark

        # 어깨·힙 랜드마크 신뢰도 확인
        required_idx = [_L_SHOULDER, _R_SHOULDER, _L_HIP, _R_HIP]
        if any(lm[i].visibility < _VIS_THRESHOLD for i in required_idx):
            self._confirm_cnt = 0
            return

        # 어깨 중점 → 힙 중점 벡터
        shoulder_x = (lm[_L_SHOULDER].x + lm[_R_SHOULDER].x) / 2.0
        shoulder_y = (lm[_L_SHOULDER].y + lm[_R_SHOULDER].y) / 2.0
        hip_x      = (lm[_L_HIP].x      + lm[_R_HIP].x)      / 2.0
        hip_y      = (lm[_L_HIP].y      + lm[_R_HIP].y)      / 2.0

        dx = hip_x - shoulder_x
        dy = hip_y - shoulder_y
        if abs(dx) < 1e-6 and abs(dy) < 1e-6:
            self._confirm_cnt = 0
            return

        # 수직 기준 각도 (0° = 서있음, 90° = 완전히 누워있음)
        angle_deg = abs(math.degrees(math.atan2(abs(dx), abs(dy))))
        is_lying  = angle_deg > self._angle_thresh

        if is_lying:
            self._confirm_cnt += 1
        else:
            self._confirm_cnt = 0
            return

        # 연속 confirm_limit 프레임 + 쿨다운 경과 → 경고 발행
        wall_now = time.time()
        if (self._confirm_cnt >= self._confirm_limit
                and wall_now - self._last_alert_t >= self._cooldown_sec):
            self._last_alert_t = wall_now
            self._confirm_cnt  = 0
            self._publish_alert(angle_deg)

    # ── 발행 헬퍼 ─────────────────────────────────────────────────────────

    def _publish_alert(self, angle_deg: float):
        payload = json.dumps({
            'detected':  True,
            'angle_deg': round(angle_deg, 1),
            'timestamp': round(time.time(), 3),
        })
        msg      = String()
        msg.data = payload
        self._alert_pub.publish(msg)
        self.get_logger().warn(
            f'[낙상 감지] 누워있음 판정 (각도={angle_deg:.1f}°)')

    def _publish_status(self):
        wall_now  = time.time()
        remaining = max(0.0, self._cooldown_sec - (wall_now - self._last_alert_t))
        payload   = json.dumps({
            'enabled':            self._enabled,
            'model_ok':           self._pose is not None,
            'cooldown_remaining': round(remaining, 1),
        })
        msg      = String()
        msg.data = payload
        self._status_pub.publish(msg)

    def destroy_node(self):
        if self._pose:
            self._pose.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = FallDetectionNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
