#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import CompressedImage
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
import urllib.request

class MJPEGBridge(Node):
    def __init__(self):
        super().__init__('camera_bridge')
        
        self.declare_parameter('stream_url', 'http://192.168.0.33:8000/video')
        url = self.get_parameter('stream_url').value
        
        # QoS 설정
        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )
        
        self.publisher = self.create_publisher(
            CompressedImage, 
            '/camera/image_raw/compressed', 
            qos
        )
        
        self.get_logger().info(f'Connecting to: {url}')
        self.stream = urllib.request.urlopen(url, timeout=10)
        self.bytes_data = b''
        
        # 더 자주 읽기
        self.timer = self.create_timer(0.01, self.timer_callback)  # 100Hz
        self.frame_count = 0
        self.get_logger().info('MJPEG bridge started (compressed mode)')
    
    def timer_callback(self):
        try:
            # 더 많이 읽기
            self.bytes_data += self.stream.read(16384)
            
            a = self.bytes_data.find(b'\xff\xd8')
            b = self.bytes_data.find(b'\xff\xd9')
            
            if a != -1 and b != -1:
                jpg = self.bytes_data[a:b+2]
                self.bytes_data = self.bytes_data[b+2:]
                
                # 디코딩 없이 직접 전송
                msg = CompressedImage()
                msg.header.stamp = self.get_clock().now().to_msg()
                msg.header.frame_id = 'camera'
                msg.format = 'jpeg'
                msg.data = list(jpg)  # bytes를 list로
                
                self.publisher.publish(msg)
                
                self.frame_count += 1
                if self.frame_count % 300 == 0:
                    self.get_logger().info(f'Published {self.frame_count} frames')
                    
        except Exception as e:
            self.get_logger().error(f'Error: {e}')

def main(args=None):
    rclpy.init(args=args)
    node = MJPEGBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
