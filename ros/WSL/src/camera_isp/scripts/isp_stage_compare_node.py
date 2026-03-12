#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import cv2
import numpy as np
import os
from sensor_msgs.msg import CompressedImage

# 스냅샷 저장 경로 (camera_isp 패키지 폴더 내부)
SAVE_DIR = '/root/ros2_ws/src/camera_isp/snapshots'

class ISPStageCompareNode(Node):
    def __init__(self):
        super().__init__('isp_stage_compare_node')

        self.subscription = self.create_subscription(
            CompressedImage,
            '/camera/image_raw/compressed',
            self.image_callback,
            10
        )
        os.makedirs(SAVE_DIR, exist_ok=True)
        self.last_log_time = None   # None이면 즉시 출력
        self.get_logger().info(f'ISP Stage Compare Node 시작! (30초마다 출력, 스냅샷: {SAVE_DIR})')

    # ── 필터 파이프라인 (독립 적용) ───────────────────────────────────────────

    def apply_bilateral(self, img):
        """Bilateral Filter 단독 적용: 에지 보존 노이즈 제거"""
        return cv2.bilateralFilter(img, 5, 25, 25)

    def apply_lut(self, img, gamma=0.8):
        """CLAHE 대체: LUT(정답지) 기반 초고속 감마(대비/밝기) 교정"""
        # LUT 테이블 생성 (미리 계산)
        lut = np.array([((i / 255.0) ** gamma) * 255 for i in range(256)]).astype("uint8")
        
        # BGR -> YCrCb 변환 후 Y채널에 LUT 적용
        ycrcb = cv2.cvtColor(img, cv2.COLOR_BGR2YCrCb)
        channels = list(cv2.split(ycrcb))
        channels[0] = cv2.LUT(channels[0], lut)
        ycrcb = cv2.merge(channels)
        return cv2.cvtColor(ycrcb, cv2.COLOR_YCrCb2BGR)

    def apply_unsharp(self, img, alpha=0.5):
        """Unsharp Mask 대체: 3x3 Laplacian 에지 커널 매핑 (1-pass)"""
        # 십자가 모양 통과 필터 배열 = [0, -α, 0] / [-α, 1+4α, -α] / [0, -α, 0]
        kernel = np.array([
            [0.0, -alpha, 0.0],
            [-alpha, 1.0 + 4.0 * alpha, -alpha],
            [0.0, -alpha, 0.0]
        ], dtype=np.float32)
        return cv2.filter2D(img, -1, kernel)

    # ── 지표 계산 ─────────────────────────────────────────────────────────────

    def calculate_metrics(self, img):
        gray_u8  = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        gray     = gray_u8.astype(np.float32)

        # 1. 선명도 (Sharpness): Laplacian 분산
        sharpness = cv2.Laplacian(gray_u8, cv2.CV_64F).var()

        # 2. 대비 (Contrast): 픽셀 밝기 표준편차
        contrast = float(gray.std())

        # 3. 밝기 (Brightness): 픽셀 밝기 평균
        brightness = float(gray.mean())

        # 4. 정보량 (Entropy): Shannon 엔트로피
        hist = cv2.calcHist([gray_u8], [0], None, [256], [0, 256])
        hist = hist.flatten() / hist.sum()
        hist = hist[hist > 0]
        entropy = float(-np.sum(hist * np.log2(hist)))

        # 5. 노이즈 추정치
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        noise   = float(np.std(gray - blurred))

        return sharpness, contrast, brightness, entropy, noise

    # ── 스냅샷 저장 ───────────────────────────────────────────────────────────

    def save_snapshots(self, images: dict):
        """4개 이미지를 각각(고정 파일명) + 2x2 격자 PNG로 저장. 매번 덮어씀."""
        h, w = list(images.values())[0].shape[:2]

        labels_for_grid = ['Raw (720p)', 'Bilateral Filter', 'Bilateral+LUT Gamma', 'Full Pipeline (Bilateral+LUT+Sharpen)']
        imgs_for_grid = []

        for (key, img), lbl in zip(images.items(), labels_for_grid):
            # 고정 파일명으로 저장 (덮어쓰기)
            path = os.path.join(SAVE_DIR, f'{key}.png')
            cv2.imwrite(path, img)

            # 레이블을 이미지 위에 합성
            labeled = img.copy()
            cv2.rectangle(labeled, (0, 0), (w, 34), (0, 0, 0), -1)
            cv2.putText(labeled, lbl, (8, 24), cv2.FONT_HERSHEY_SIMPLEX,
                        0.7, (255, 255, 255), 2, cv2.LINE_AA)
            imgs_for_grid.append(labeled)

        # 2x2 그리드 합성 이미지 (고정 파일명)
        top    = np.hstack([imgs_for_grid[0], imgs_for_grid[1]])
        bottom = np.hstack([imgs_for_grid[2], imgs_for_grid[3]])
        grid_path = os.path.join(SAVE_DIR, 'grid.png')
        cv2.imwrite(grid_path, np.vstack([top, bottom]))

        return grid_path

    # ── 메인 콜백 ─────────────────────────────────────────────────────────────

    def image_callback(self, msg):
        raw_np = np.frombuffer(msg.data, np.uint8)
        raw_img = cv2.imdecode(raw_np, cv2.IMREAD_COLOR)

        if raw_img is None:
            self.get_logger().warning('이미지 디코딩 실패')
            return

        # [최적화] 1080p 원본을 720p로 Downscaling 하여 연산량 55% 감소
        if raw_img.shape[1] > 1280:
            raw_img = cv2.resize(raw_img, (1280, 720), interpolation=cv2.INTER_AREA)

        # 쓰로틀링: 처음에는 즉시, 이후 30초마다 1회 실행
        now = self.get_clock().now()
        if self.last_log_time is not None:
            elapsed = (now - self.last_log_time).nanoseconds / 1e9
            if elapsed < 30.0:
                return
        self.last_log_time = now

        # 각 단계 누적 적용 (isp_node.cpp 실제 파이프라인과 동일: bilateral → LUT → sharpen)
        # 수정 전: 각 필터를 raw에 독립적으로 적용 → 실제 파이프라인과 다른 결과
        # 수정 후: 누적 적용 → 실제 ISP 처리 과정을 정확하게 시각화
        filtered_img  = self.apply_bilateral(raw_img)    # 1단계: raw → bilateral
        lut_img       = self.apply_lut(filtered_img)      # 2단계: bilateral → LUT
        sharpened_img = self.apply_unsharp(lut_img)       # 3단계: LUT → sharpen

        images = {
            'raw':    raw_img,
            'filter': filtered_img,
            'lut':    lut_img,
            'sharpen': sharpened_img,
        }

        # 지표 계산
        m_raw  = self.calculate_metrics(raw_img)
        m_flt  = self.calculate_metrics(filtered_img)
        m_lut  = self.calculate_metrics(lut_img)
        m_shp  = self.calculate_metrics(sharpened_img)

        labels   = ['Sharpness ', 'Contrast  ', 'Brightness', 'Entropy   ', 'Noise Est ']
        versions = [m_raw, m_flt, m_lut, m_shp]

        # ── 절대값 테이블 ──
        header = f"{'Metric':<12}| {'Raw':>8} | {'+Filter':>8} | {'+LUT Gamma':>10} | {'+Sharpen':>9}"
        sep    = '-' * len(header)
        rows_abs = []
        for i, lbl in enumerate(labels):
            vals = [v[i] for v in versions]
            rows_abs.append(
                f"{lbl:<12}| {vals[0]:8.2f} | {vals[1]:8.2f} | {vals[2]:8.2f} | {vals[3]:9.2f}"
            )

        # ── 변화율 테이블 ──
        rows_diff = []
        for i, lbl in enumerate(labels):
            raw_v = m_raw[i]
            diffs = []
            for v in [m_flt, m_lut, m_shp]:
                d = ((v[i] - raw_v) / raw_v * 100) if raw_v != 0 else 0.0
                diffs.append(f"{d:+8.1f}%")
            rows_diff.append(
                f"{lbl:<12}|{'':>9}| {diffs[0]} | {diffs[1]} | {diffs[2]}"
            )

        # ── 스냅샷 저장 (고정 파일명, 덮어쓰기) ──
        grid_path = self.save_snapshots(images)

        log_msg = (
            f"\n{'='*len(header)}\n"
            f" ISP Stage Comparison (vs Raw)\n"
            f"{'='*len(header)}\n"
            f"{header}\n{sep}\n"
            + '\n'.join(rows_abs) +
            f"\n{sep}\n"
            f" Change vs Raw:\n{sep}\n"
            + '\n'.join(rows_diff) +
            f"\n{'='*len(header)}\n"
            f" 📷 스냅샷 저장: {SAVE_DIR}/{{raw,filter,lut,sharpen,grid}}.png\n"
            f"{'='*len(header)}"
        )
        self.get_logger().info(log_msg)


def main(args=None):
    rclpy.init(args=args)
    node = ISPStageCompareNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
