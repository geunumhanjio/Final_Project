#!/usr/bin/env python3
"""
person_tracker_node.py
======================
MediaPipe Pose 기반 사람 추종 노드 (WSL 측, CPU 전용)

동작:
  1. /camera/image_raw/compressed 구독 (설정 가능)
  2. MediaPipe Pose로 사람 감지 (최대 max_fps 제한)
  3. 어깨 중점 기준:
     - X 오프셋 → /cmd_vel (차체 회전으로 수평 추종)
     - Y 오프셋 → /camera/tilt (서보로 수직 추종)
  4. 추종 상태 → /tracking/status (JSON String)

구독:
  /camera/image_raw/compressed  (sensor_msgs/CompressedImage)  설정 가능
  /tracking/enable               (std_msgs/Bool)  추종 ON/OFF

발행:
  /camera/tilt      (std_msgs/Float32)         서보 틸트 각도 명령 (deg)
  /cmd_vel          (geometry_msgs/Twist)       차체 회전 명령 (추종 시)
  /tracking/status  (std_msgs/String, JSON)     추종 상태

의존성:
  pip install mediapipe opencv-python-headless
"""

import json
import time

import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage
from std_msgs.msg import Bool, Float32, String
from geometry_msgs.msg import Twist

try:
    import mediapipe as mp
    MP_AVAILABLE = True
except ImportError:
    MP_AVAILABLE = False

# ── 추종 제어 상수 ────────────────────────────────────────────────────────────
PAN_DEAD_ZONE          = 0.08   # 수평 오프셋 데드존 (화면 폭 비율)
TILT_DEAD_ZONE         = 0.08   # 수직 오프셋 데드존 (화면 높이 비율)
DIST_DEAD_ZONE         = 0.04   # 거리 데드존 (어깨 너비 비율)
PAN_GAIN               = 0.6    # 수평 오프셋 → angular_z 게인
TILT_GAIN              = 25.0   # 수직 오프셋 → 틸트 각도 게인 (deg)
LINEAR_GAIN            = 0.4    # 거리 오차 → linear_x 게인
MAX_ANGULAR_Z          = 0.35   # rad/s
MAX_LINEAR_X           = 0.15   # m/s
TARGET_SHOULDER_WIDTH  = 0.25   # 목표 어깨 너비 (정규화, 약 1m 거리 기준)
TILT_MIN         =  15.0   # deg (절대각, 서보 범위와 동일)
TILT_MAX         =  90.0   # deg (절대각, 서보 범위와 동일)
TILT_CENTER      =  32.5   # deg (중립 위치)

# MediaPipe Pose 랜드마크 인덱스
_L_SHOULDER = 11
_R_SHOULDER = 12


class PersonTrackerNode(Node):

    def __init__(self):
        super().__init__('person_tracker_node')

        # ── 파라미터 ──────────────────────────────────────────────────────
        self.declare_parameter('max_fps',            10.0)
        self.declare_parameter('visibility_threshold', 0.5)   # 랜드마크 신뢰도 임계값
        self.declare_parameter('pan_gain',             PAN_GAIN)
        self.declare_parameter('tilt_gain',            TILT_GAIN)
        self.declare_parameter('linear_gain',          LINEAR_GAIN)
        self.declare_parameter('pan_dead_zone',        PAN_DEAD_ZONE)
        self.declare_parameter('tilt_dead_zone',       TILT_DEAD_ZONE)
        self.declare_parameter('dist_dead_zone',       DIST_DEAD_ZONE)
        self.declare_parameter('target_shoulder_width', TARGET_SHOULDER_WIDTH)
        self.declare_parameter('image_topic',          '/camera/image_raw/compressed')

        self._max_interval       = 1.0 / self.get_parameter('max_fps').value
        self._vis_thresh         = self.get_parameter('visibility_threshold').value
        self._pan_gain           = self.get_parameter('pan_gain').value
        self._tilt_gain          = self.get_parameter('tilt_gain').value
        self._linear_gain        = self.get_parameter('linear_gain').value
        self._pan_dz             = self.get_parameter('pan_dead_zone').value
        self._tilt_dz            = self.get_parameter('tilt_dead_zone').value
        self._dist_dz            = self.get_parameter('dist_dead_zone').value
        self._target_sw          = self.get_parameter('target_shoulder_width').value
        image_topic              = self.get_parameter('image_topic').value

        # ── MediaPipe Pose 초기화 ─────────────────────────────────────────
        self._pose = None
        if MP_AVAILABLE:
            self._pose = mp.solutions.pose.Pose(
                static_image_mode=False,
                model_complexity=0,        # 0=Lite, 1=Full, 2=Heavy → CPU는 0
                smooth_landmarks=True,
                min_detection_confidence=self._vis_thresh,
                min_tracking_confidence=self._vis_thresh,
            )
            self.get_logger().info('MediaPipe Pose 초기화 완료 (Lite 모드)')
        else:
            self.get_logger().error('mediapipe 미설치: pip install mediapipe')

        # ── 상태 ──────────────────────────────────────────────────────────
        self._enabled      = False
        self._last_infer_t = 0.0
        self._cur_tilt     = TILT_CENTER

        # ── Publishers ────────────────────────────────────────────────────
        self._tilt_pub    = self.create_publisher(Float32, '/camera/tilt',       10)
        self._cmdvel_pub  = self.create_publisher(Twist,   '/cmd_vel',           10)
        self._status_pub  = self.create_publisher(String,  '/tracking/status',   10)
        self._nav_cmd_pub = self.create_publisher(String,  '/nav/command',       10)

        # ── Subscribers ───────────────────────────────────────────────────
        self._img_sub = self.create_subscription(
            CompressedImage, image_topic, self._image_callback, 10)

        self._enable_sub = self.create_subscription(
            Bool, '/tracking/enable', self._enable_callback, 10)

        self.create_timer(1.0, self._publish_status)

        self.get_logger().info(
            f'PersonTrackerNode 시작 (max {self.get_parameter("max_fps").value}fps, '
            f'image={image_topic}). /tracking/enable 로 활성화하세요.')

    # ── 추종 ON/OFF ───────────────────────────────────────────────────────────

    def _enable_callback(self, msg: Bool):
        self._enabled = msg.data
        if self._enabled:
            cancel_msg = String()
            cancel_msg.data = json.dumps({'cmd': 'cancel'})
            self._nav_cmd_pub.publish(cancel_msg)
            self.get_logger().info('자율주행 cancel → 사람 추종 모드')
        else:
            self._stop_robot()
            self._reset_tilt()
        self.get_logger().info(f'추종 {"활성화" if self._enabled else "비활성화"}')

    # ── 이미지 콜백 ───────────────────────────────────────────────────────────

    def _image_callback(self, msg: CompressedImage):
        if not self._enabled or self._pose is None:
            return

        now = time.monotonic()
        if now - self._last_infer_t < self._max_interval:
            return
        self._last_infer_t = now

        np_arr = np.frombuffer(msg.data, np.uint8)
        frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
        if frame is None:
            return

        # MediaPipe는 RGB 입력
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        try:
            results = self._pose.process(frame_rgb)
        except Exception as e:
            self.get_logger().warn(f'MediaPipe 추론 오류: {e}')
            return

        if results.pose_landmarks is None:
            self._stop_robot()
            return

        lm = results.pose_landmarks.landmark
        ls = lm[_L_SHOULDER]
        rs = lm[_R_SHOULDER]

        # 어깨 랜드마크 신뢰도 확인
        if ls.visibility < self._vis_thresh or rs.visibility < self._vis_thresh:
            self._stop_robot()
            return

        # 어깨 중점 (정규화 좌표 0~1)
        cx = (ls.x + rs.x) / 2.0
        cy = (ls.y + rs.y) / 2.0

        # 화면 중심 기준 오프셋 (-0.5 ~ +0.5)
        x_offset = cx - 0.5
        y_offset = cy - 0.5

        # 수평 제어 → cmd_vel angular_z
        if abs(x_offset) > self._pan_dz:
            angular_z = -float(np.clip(x_offset * self._pan_gain,
                                       -MAX_ANGULAR_Z, MAX_ANGULAR_Z))
        else:
            angular_z = 0.0

        # 거리 제어 → cmd_vel linear_x (어깨 너비 기반)
        shoulder_width = abs(ls.x - rs.x)
        dist_error = shoulder_width - self._target_sw  # 양수: 너무 가까움, 음수: 너무 멀음
        if abs(dist_error) > self._dist_dz:
            linear_x = -float(np.clip(dist_error * self._linear_gain,
                                      -MAX_LINEAR_X, MAX_LINEAR_X))
        else:
            linear_x = 0.0

        # 수직 제어 → /camera/tilt
        # 20°=위, 90°=아래이므로: 사람이 위에 있으면(y_offset<0) 각도 감소(위로)
        if abs(y_offset) > self._tilt_dz:
            tilt_delta = y_offset * self._tilt_gain
            self._cur_tilt = float(np.clip(
                self._cur_tilt + tilt_delta, TILT_MIN, TILT_MAX))

        self._send_tilt(self._cur_tilt)
        self._send_cmdvel(linear_x, angular_z)

    # ── 발행 헬퍼 ────────────────────────────────────────────────────────────

    def _send_tilt(self, angle_deg: float):
        msg = Float32()
        msg.data = angle_deg
        self._tilt_pub.publish(msg)

    def _send_cmdvel(self, linear_x: float, angular_z: float):
        msg = Twist()
        msg.linear.x  = linear_x
        msg.angular.z = angular_z
        self._cmdvel_pub.publish(msg)

    def _stop_robot(self):
        self._send_cmdvel(0.0, 0.0)

    def _reset_tilt(self):
        self._cur_tilt = TILT_CENTER
        self._send_tilt(TILT_CENTER)

    def _publish_status(self):
        payload = json.dumps({
            'enabled':        self._enabled,
            'tilt_deg':       round(self._cur_tilt, 2),
            'model_ok':       self._pose is not None,
            'target_shoulder_width': self._target_sw,
        })
        msg = String()
        msg.data = payload
        self._status_pub.publish(msg)

    def destroy_node(self):
        if self._pose:
            self._pose.close()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)
    node = PersonTrackerNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
