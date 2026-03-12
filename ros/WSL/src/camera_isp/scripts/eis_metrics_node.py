#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage
import cv2
import numpy as np

class EISMetricsNode(Node):
    def __init__(self):
        super().__init__('eis_metrics_node')

        # Subscriptions
        self.raw_sub = self.create_subscription(
            CompressedImage,
            '/camera/image_raw/compressed',
            self.raw_callback,
            qos_profile=rclpy.qos.qos_profile_sensor_data
        )
        self.eis_sub = self.create_subscription(
            CompressedImage,
            '/camera/image_eis/compressed',
            self.eis_callback,
            qos_profile=rclpy.qos.qos_profile_sensor_data
        )

        # 상태 변수
        self.raw_prev_gray = None
        self.raw_prev_pts = None
        self.eis_prev_gray = None
        self.eis_prev_pts = None

        # 누적 변수 (1초마다 평균 계산 후 리셋)
        self.raw_jitter_accum = 0.0
        self.raw_frame_count = 0
        self.eis_jitter_accum = 0.0
        self.eis_frame_count = 0

        # Lucas-Kanade 옵티컬 플로우 설정
        self.lk_params = dict(winSize=(21, 21), maxLevel=3,
                              criteria=(cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 30, 0.01))
        
        # 특징점 추출 설정
        self.feature_params = dict(maxCorners=100, qualityLevel=0.1, minDistance=30, blockSize=7)

        # 5초 주기로 결과 출력
        self.timer = self.create_timer(5.0, self.timer_callback)
        self.get_logger().info('📸 EIS Metrics Node Started! (Calculating Optical Flow Jitter...)')

    def calculate_jitter(self, img_msg, prev_gray, prev_pts):
        """
        옵티컬 플로우를 이용하여 프레임 간 픽셀 평균 이동량(Jitter) 측정
        반환값: (이번 프레임 지터, 갱신된 prev_gray, 갱신된 prev_pts)
        """
        # 1. 디코딩 및 그레이스케일 변환
        np_arr = np.frombuffer(img_msg.data, np.uint8)
        frame = cv2.imdecode(np_arr, cv2.IMREAD_GRAYSCALE)
        if frame is None:
            return 0.0, prev_gray, prev_pts

        # 사이즈 통일 (720p 기준으로 통일)
        if frame.shape[1] > 1280:
            frame = cv2.resize(frame, (1280, 720), interpolation=cv2.INTER_AREA)

        current_jitter = 0.0

        # 2. 첫 프레임이거나 잃어버린 특징점이 너무 많으면 새로 특징점 추출
        if prev_gray is None or prev_pts is None or len(prev_pts) < 10:
            new_pts = cv2.goodFeaturesToTrack(frame, mask=None, **self.feature_params)
            return 0.0, frame, new_pts

        # 3. 옵티컬 플로우(Lucas-Kanade) 계산
        next_pts, status, err = cv2.calcOpticalFlowPyrLK(prev_gray, frame, prev_pts, None, **self.lk_params)

        if next_pts is not None and status is not None:
            # 유효한 특징점만 골라냄
            good_new = next_pts[status == 1]
            good_old = prev_pts[status == 1]

            if len(good_new) > 0:
                # 유효 특징점들의 L2 거리(픽셀 이동량) 계산
                distances = np.linalg.norm(good_new - good_old, axis=1)
                
                # 상위/하위 10% 아웃라이어 제거 (물체가 지나가서 생기는 플로우 방지, 진짜 카메라 흔들림만 측정)
                if len(distances) > 4:
                    percentile_10 = np.percentile(distances, 10)
                    percentile_90 = np.percentile(distances, 90)
                    filtered_dist = distances[(distances >= percentile_10) & (distances <= percentile_90)]
                    if len(filtered_dist) > 0:
                        current_jitter = np.mean(filtered_dist)
                else:
                    current_jitter = np.mean(distances)

            # 다음 프레임을 위한 업데이트
            # 특징점이 절반 이하로 줄어들었으면 새로 뽑기
            if len(good_new) < 30:
                good_new = cv2.goodFeaturesToTrack(frame, mask=None, **self.feature_params)
            else:
                good_new = good_new.reshape(-1, 1, 2)
                
            return current_jitter, frame, good_new
            
        # 플로우 계산 실패 시 리셋
        return 0.0, frame, None


    def raw_callback(self, msg):
        jitter, self.raw_prev_gray, self.raw_prev_pts = self.calculate_jitter(msg, self.raw_prev_gray, self.raw_prev_pts)
        self.raw_jitter_accum += jitter
        self.raw_frame_count += 1

    def eis_callback(self, msg):
        jitter, self.eis_prev_gray, self.eis_prev_pts = self.calculate_jitter(msg, self.eis_prev_gray, self.eis_prev_pts)
        self.eis_jitter_accum += jitter
        self.eis_frame_count += 1

    def timer_callback(self):
        # 데이터가 없으면 무시
        if self.raw_frame_count == 0 or self.eis_frame_count == 0:
            return

        # 평균 Jitter 계산 (픽셀 단위/프레임)
        avg_raw_jitter = self.raw_jitter_accum / self.raw_frame_count
        avg_eis_jitter = self.eis_jitter_accum / self.eis_frame_count

        # 떨림 감소율 (%) 계산
        reduction_rate = 0.0
        if avg_raw_jitter > 0.01:
            reduction_rate = ((avg_raw_jitter - avg_eis_jitter) / avg_raw_jitter) * 100.0

        # 터미널 출력
        log_msg = (
            f"\n----------------------------------------\n"
            f" 📊 EIS Performance Metrics (Last 5s)\n"
            f"----------------------------------------\n"
            f" [RAW Camera] Avg Jitter : {avg_raw_jitter:5.2f} px/frame\n"
            f" [EIS Camera] Avg Jitter : {avg_eis_jitter:5.2f} px/frame\n"
            f"----------------------------------------\n"
        )
        
        if reduction_rate > 0:
            log_msg += f" ✨ Jitter Reduction :  -{abs(reduction_rate):.1f}% (흔들림 완화)\n"
        else:
            log_msg += f" ⚠️ Jitter Reduction :   {abs(reduction_rate):+.1f}% (차이 없음/마이너스)\n"
            
        log_msg += f"----------------------------------------"
        
        self.get_logger().info(log_msg)

        # 누적기 초기화
        self.raw_jitter_accum = 0.0
        self.raw_frame_count = 0
        self.eis_jitter_accum = 0.0
        self.eis_frame_count = 0


def main(args=None):
    rclpy.init(args=args)
    node = EISMetricsNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.try_shutdown()

if __name__ == '__main__':
    main()
