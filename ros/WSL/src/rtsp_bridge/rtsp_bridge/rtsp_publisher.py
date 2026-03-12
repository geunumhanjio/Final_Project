#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage
import cv2
import numpy as np

class RTSPPublisher(Node):
    def __init__(self):
        super().__init__('rtsp_publisher')
        
        # 파라미터 선언
        self.declare_parameter('input_topic', '/camera/image_raw/compressed')
        self.declare_parameter('rtsp_host', 'host.docker.internal')
        self.declare_parameter('rtsp_port', 8554)
        self.declare_parameter('stream_name', 'camera')
        self.declare_parameter('width', 640)
        self.declare_parameter('height', 480)
        self.declare_parameter('fps', 30)
        self.declare_parameter('bitrate', 2000)
        
        # 파라미터 가져오기
        input_topic = self.get_parameter('input_topic').value
        rtsp_host = self.get_parameter('rtsp_host').value
        rtsp_port = self.get_parameter('rtsp_port').value
        stream_name = self.get_parameter('stream_name').value
        self._target_width = self.get_parameter('width').value
        self._target_height = self.get_parameter('height').value
        fps = self.get_parameter('fps').value
        bitrate = self.get_parameter('bitrate').value
        
        rtsp_url = f'rtsp://{rtsp_host}:{rtsp_port}/{stream_name}'
        
        self.frame_count = 0
        
        # GStreamer 파이프라인
        gst_pipeline = (
            f'appsrc ! videoconvert ! '
            f'video/x-raw,format=I420,width={self._target_width},height={self._target_height},framerate={fps}/1 ! '
            f'x264enc speed-preset=ultrafast tune=zerolatency bitrate={bitrate} key-int-max={fps} ! '
            f'rtspclientsink location={rtsp_url}'
        )
        
        self.get_logger().info(f'GStreamer pipeline: {gst_pipeline}')
        
        # VideoWriter 생성
        self.writer = cv2.VideoWriter(
            gst_pipeline,
            cv2.CAP_GSTREAMER,
            0,
            fps,
            (self._target_width, self._target_height),
            True
        )
        
        if not self.writer.isOpened():
            self.get_logger().error('Failed to open GStreamer pipeline!')
            self.get_logger().error('Check:')
            self.get_logger().error('  1. GStreamer plugins installed')
            self.get_logger().error('  2. mediamtx is running')
            self.get_logger().error(f'  3. Can reach {rtsp_url}')
            raise RuntimeError('VideoWriter initialization failed')
        
        # ROS 구독자 - CompressedImage 사용
        self.subscription = self.create_subscription(
            CompressedImage,
            input_topic,
            self.image_callback,
            10
        )
        
        self.get_logger().info('='*60)
        self.get_logger().info('RTSP Publisher Started! (Compressed Mode)')
        self.get_logger().info(f'  Input topic: {input_topic}')
        self.get_logger().info(f'  RTSP URL: {rtsp_url}')
        self.get_logger().info(f'  Resolution: {self._target_width}x{self._target_height} @ {fps} fps')
        self.get_logger().info(f'  Bitrate: {bitrate} kbps')
        self.get_logger().info('='*60)
        self.get_logger().info(f'View stream at: rtsp://<your_ip>:{rtsp_port}/{stream_name}')
        self.get_logger().info('='*60)
    
    def image_callback(self, msg):
        try:
            # CompressedImage(JPEG) → numpy array
            np_arr = np.frombuffer(msg.data, np.uint8)
            
            # JPEG 디코딩
            cv_image = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
            
            if cv_image is None:
                self.get_logger().error('Failed to decode JPEG image')
                return
            
            # 해상도 조정 (파라미터 조회 오버헤드 제거 - 캐싱된 변수 사용)
            if cv_image.shape[1] != self._target_width or cv_image.shape[0] != self._target_height:
                cv_image = cv2.resize(cv_image, (self._target_width, self._target_height))
            
            # RTSP로 전송
            self.writer.write(cv_image)
            
            self.frame_count += 1
            if self.frame_count % 100 == 0:
                self.get_logger().info(f'Streamed {self.frame_count} frames', throttle_duration_sec=5.0)
                
        except Exception as e:
            self.get_logger().error(f'Error: {e}')
    
    def destroy_node(self):
        if hasattr(self, 'writer') and self.writer.isOpened():
            self.writer.release()
            self.get_logger().info('VideoWriter released')
        super().destroy_node()

def main(args=None):
    rclpy.init(args=args)
    
    try:
        node = RTSPPublisher()
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    except Exception as e:
        print(f'Error: {e}')
    finally:
        rclpy.try_shutdown()

if __name__ == '__main__':
    main()
