#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import cv2
import numpy as np
import message_filters
from sensor_msgs.msg import CompressedImage

class ISPMetricsNode(Node):
    def __init__(self):
        super().__init__('isp_metrics_node')
        
        # Subscribe to both topics
        self.raw_sub = message_filters.Subscriber(self, CompressedImage, '/camera/image_raw/compressed')
        self.isp_sub = message_filters.Subscriber(self, CompressedImage, '/camera/image_isp/compressed')
        
        # Synchronize topics based on exact timestamp match
        self.ts = message_filters.TimeSynchronizer([self.raw_sub, self.isp_sub], 10)
        self.ts.registerCallback(self.image_callback)
        
        self.get_logger().info('ISP Metrics Node is running and waiting for synchronized images...')
        self.last_log_time = self.get_clock().now()
        self.first_frame = True
        
    def calculate_metrics(self, img):
        gray_u8  = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        gray     = gray_u8.astype(np.float32)

        # 1. Contrast (RMS Contrast -> numpy standard deviation of pixel intensities)
        contrast = float(gray.std())
        
        # 2. Sharpness (Laplacian Variance)
        sharpness = cv2.Laplacian(gray_u8, cv2.CV_64F).var()
        
        # 3. SNR (Signal to Noise Ratio in dB)
        mean_val = float(gray.mean())
        snr = 0.0
        if contrast > 0 and mean_val > 0:
            snr = 20 * np.log10(mean_val / contrast)
            
        # 4. Brightness
        brightness = mean_val
        
        # 5. Entropy
        hist = cv2.calcHist([gray_u8], [0], None, [256], [0, 256])
        hist = hist.flatten() / hist.sum()
        hist = hist[hist > 0]
        entropy = float(-np.sum(hist * np.log2(hist)))
        
        # 6. Noise Estimate
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        noise = float(np.std(gray - blurred))
            
        return sharpness, contrast, brightness, entropy, noise

    def image_callback(self, raw_msg, isp_msg):
        # Decode raw image
        raw_np = np.frombuffer(raw_msg.data, np.uint8)
        raw_img = cv2.imdecode(raw_np, cv2.IMREAD_COLOR)
        
        # Decode isp image
        isp_np = np.frombuffer(isp_msg.data, np.uint8)
        isp_img = cv2.imdecode(isp_np, cv2.IMREAD_COLOR)
        
        if raw_img is None or isp_img is None:
            self.get_logger().warning('Failed to decode images.')
            return

        # 쓰로틀링: 30초마다 한 번씩만 출력 (단, 첫 프레임은 즉시 출력)
        now = self.get_clock().now()
        if not self.first_frame and (now - self.last_log_time).nanoseconds / 1e9 < 30.0:
            return
        
        self.last_log_time = now
        self.first_frame = False

        # Calculate metrics (sharpness, contrast, brightness, entropy, noise)
        m_raw = self.calculate_metrics(raw_img)
        m_isp = self.calculate_metrics(isp_img)
        
        labels   = ['Sharpness ', 'Contrast  ', 'Brightness', 'Entropy   ', 'Noise Est ']
        
        # Format the output similar to the stage compare node but just for Raw vs ISP
        header = f"{'Metric':<12}| {'Raw':>8} | {'ISP':>9} | {'Diff':>8}"
        sep    = '-' * len(header)
        
        rows = []
        for i, lbl in enumerate(labels):
            raw_val = m_raw[i]
            isp_val = m_isp[i]
            diff_pct = ((isp_val - raw_val) / raw_val * 100.0) if raw_val > 0 else 0.0
            
            rows.append(f"{lbl:<12}| {raw_val:8.2f} | {isp_val:9.2f} | {diff_pct:+7.1f}%")
            
        # Append SNR which is derived
        # SNR = 20 * log10(Brightness / Contrast)
        raw_snr = 0.0
        if m_raw[1] > 0 and m_raw[2] > 0: raw_snr = 20 * np.log10(m_raw[2] / m_raw[1])
        isp_snr = 0.0
        if m_isp[1] > 0 and m_isp[2] > 0: isp_snr = 20 * np.log10(m_isp[2] / m_isp[1])
        snr_diff = ((isp_snr - raw_snr) / raw_snr * 100.0) if raw_snr > 0 else 0.0
        rows.append(f"{'SNR (dB)  ':<12}| {raw_snr:8.2f} | {isp_snr:9.2f} | {snr_diff:+7.1f}%")

        log_msg = (
            f"\n{'='*len(header)}\n"
            f" ISP Live Metrics (30s interval)\n"
            f"{'='*len(header)}\n"
            f"{header}\n{sep}\n"
            + '\n'.join(rows) +
            f"\n{'='*len(header)}"
        )
        
        self.get_logger().info(log_msg)

def main(args=None):
    rclpy.init(args=args)
    node = ISPMetricsNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
