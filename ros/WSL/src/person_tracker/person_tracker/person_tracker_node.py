#!/usr/bin/env python3
"""
person_tracker_node.py
======================
YOLOv8n 기반 사람 추종 노드 (WSL 측)

동작:
  1. /camera/image_isp/compressed 구독 (ISP 처리된 720p 이미지)
  2. YOLOv8n으로 사람(class 0) 감지 (최대 10fps 제한)
  3. 화면 내 가장 큰 사람의 위치 기준:
     - X 오프셋 → /cmd_vel (차체 회전으로 수평 추종)
     - Y 오프셋 → /camera/tilt (서보로 수직 추종)
  4. 추종 상태 → /tracking/status (JSON String)

구독:
  /camera/image_isp/compressed  (sensor_msgs/CompressedImage)
  /tracking/enable               (std_msgs/Bool)  추종 ON/OFF

발행:
  /camera/tilt      (std_msgs/Float32)         서보 틸트 각도 명령 (deg)
  /cmd_vel          (geometry_msgs/Twist)       차체 회전 명령 (추종 시)
  /tracking/status  (std_msgs/String, JSON)     추종 상태

의존성:
  pip install ultralytics opencv-python-headless
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
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False

# ── 추종 제어 상수 ────────────────────────────────────────────────────────────
PERSON_CLASS_ID  = 0       # COCO class 0 = person
PAN_DEAD_ZONE    = 0.08    # 수평 오프셋 데드존 (화면 폭 비율)
TILT_DEAD_ZONE   = 0.08    # 수직 오프셋 데드존 (화면 높이 비율)
PAN_GAIN         = 0.6     # 수평 오프셋 → angular_z 게인
TILT_GAIN        = 25.0    # 수직 오프셋 → 틸트 각도 게인 (deg)
MAX_ANGULAR_Z    = 0.35    # rad/s (수동 cmd_vel 최대값과 별도)
TILT_MIN         = -30.0   # deg
TILT_MAX         =  30.0   # deg


class PersonTrackerNode(Node):

    def __init__(self):
        super().__init__('person_tracker_node')

        # ── 파라미터 ──────────────────────────────────────────────────────
        self.declare_parameter('model',        'yolov8n.pt')  # 모델 파일/이름
        self.declare_parameter('max_fps',       10.0)          # 최대 추론 fps
        self.declare_parameter('conf_threshold', 0.5)          # 감지 신뢰도 임계값
        self.declare_parameter('pan_gain',      PAN_GAIN)
        self.declare_parameter('tilt_gain',     TILT_GAIN)
        self.declare_parameter('pan_dead_zone', PAN_DEAD_ZONE)
        self.declare_parameter('tilt_dead_zone',TILT_DEAD_ZONE)

        model_path  = self.get_parameter('model').value
        self._max_interval  = 1.0 / self.get_parameter('max_fps').value
        self._conf          = self.get_parameter('conf_threshold').value
        self._pan_gain      = self.get_parameter('pan_gain').value
        self._tilt_gain     = self.get_parameter('tilt_gain').value
        self._pan_dz        = self.get_parameter('pan_dead_zone').value
        self._tilt_dz       = self.get_parameter('tilt_dead_zone').value

        # ── YOLO 모델 로드 ────────────────────────────────────────────────
        self._model = None
        if YOLO_AVAILABLE:
            try:
                self._model = YOLO(model_path)
                # 첫 추론으로 웜업 (빈 이미지)
                self._model.predict(
                    np.zeros((480, 640, 3), dtype=np.uint8),
                    classes=[PERSON_CLASS_ID], verbose=False)
                self.get_logger().info(f'YOLOv8 모델 로드 완료: {model_path}')
            except Exception as e:
                self.get_logger().error(f'YOLO 모델 로드 실패: {e}')
                self._model = None
        else:
            self.get_logger().error(
                'ultralytics 미설치: pip install ultralytics')

        # ── 상태 ──────────────────────────────────────────────────────────
        self._enabled       = False   # /tracking/enable로 ON/OFF
        self._last_infer_t  = 0.0
        self._cur_tilt      = 0.0    # 현재 명령 틸트 각도

        # ── Publishers ────────────────────────────────────────────────────
        self._tilt_pub   = self.create_publisher(Float32, '/camera/tilt', 10)
        self._cmdvel_pub = self.create_publisher(Twist,   '/cmd_vel',     10)
        self._status_pub = self.create_publisher(String,  '/tracking/status', 10)

        # ── Subscribers ───────────────────────────────────────────────────
        self._img_sub = self.create_subscription(
            CompressedImage, '/camera/image_isp/compressed',
            self._image_callback, 10)

        self._enable_sub = self.create_subscription(
            Bool, '/tracking/enable', self._enable_callback, 10)

        # 1Hz 상태 발행
        self.create_timer(1.0, self._publish_status)

        self.get_logger().info(
            f'PersonTrackerNode 시작 (max {self.get_parameter("max_fps").value}fps, '
            f'conf={self._conf}). /tracking/enable 로 활성화하세요.')

    # ── 추종 ON/OFF ───────────────────────────────────────────────────────────

    def _enable_callback(self, msg: Bool):
        self._enabled = msg.data
        if not self._enabled:
            self._stop_robot()
            self._reset_tilt()
        self.get_logger().info(f'추종 {"활성화" if self._enabled else "비활성화"}')

    # ── 이미지 콜백 ───────────────────────────────────────────────────────────

    def _image_callback(self, msg: CompressedImage):
        if not self._enabled or self._model is None:
            return

        # fps 제한
        now = time.monotonic()
        if now - self._last_infer_t < self._max_interval:
            return
        self._last_infer_t = now

        # JPEG 디코딩
        np_arr = np.frombuffer(msg.data, np.uint8)
        frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
        if frame is None:
            return

        h, w = frame.shape[:2]

        # YOLO 추론 (사람 클래스만)
        try:
            results = self._model.predict(
                frame,
                classes=[PERSON_CLASS_ID],
                conf=self._conf,
                verbose=False,
            )
        except Exception as e:
            self.get_logger().warn(f'YOLO 추론 오류: {e}')
            return

        boxes = results[0].boxes
        if boxes is None or len(boxes) == 0:
            self._stop_robot()
            return

        # 가장 큰 바운딩 박스(면적 기준) 선택
        best = max(boxes, key=lambda b: float((b.xywh[0][2] * b.xywh[0][3])))
        cx = float(best.xywh[0][0]) / w   # 0~1 정규화 x 중심
        cy = float(best.xywh[0][1]) / h   # 0~1 정규화 y 중심

        # 화면 중심 기준 오프셋 (-0.5 ~ +0.5)
        x_offset = cx - 0.5
        y_offset = cy - 0.5

        # 수평 제어 → cmd_vel angular_z
        if abs(x_offset) > self._pan_dz:
            angular_z = -float(np.clip(x_offset * self._pan_gain,
                                       -MAX_ANGULAR_Z, MAX_ANGULAR_Z))
        else:
            angular_z = 0.0

        # 수직 제어 → /camera/tilt
        if abs(y_offset) > self._tilt_dz:
            tilt_delta = -y_offset * self._tilt_gain  # 위에 있으면 틸트 업
            self._cur_tilt = float(np.clip(
                self._cur_tilt + tilt_delta, TILT_MIN, TILT_MAX))
        # (데드존 내에서는 현재 틸트 유지)

        self._send_tilt(self._cur_tilt)
        self._send_cmdvel(angular_z)

    # ── 발행 헬퍼 ────────────────────────────────────────────────────────────

    def _send_tilt(self, angle_deg: float):
        msg = Float32()
        msg.data = angle_deg
        self._tilt_pub.publish(msg)

    def _send_cmdvel(self, angular_z: float):
        msg = Twist()
        msg.angular.z = angular_z
        self._cmdvel_pub.publish(msg)

    def _stop_robot(self):
        self._send_cmdvel(0.0)

    def _reset_tilt(self):
        self._cur_tilt = 0.0
        self._send_tilt(0.0)

    def _publish_status(self):
        payload = json.dumps({
            'enabled':   self._enabled,
            'tilt_deg':  round(self._cur_tilt, 2),
            'model_ok':  self._model is not None,
        })
        msg = String()
        msg.data = payload
        self._status_pub.publish(msg)


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
