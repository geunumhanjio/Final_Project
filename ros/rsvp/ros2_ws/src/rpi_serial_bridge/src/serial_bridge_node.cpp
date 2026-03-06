#include "rpi_serial_bridge/serial_bridge_node.hpp"
#include <fstream>
#include <ctime>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "tf2/LinearMath/Quaternion.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

namespace rpi_serial_bridge
{

SerialBridgeNode::SerialBridgeNode(const rclcpp::NodeOptions& options)
: Node("serial_bridge_node", options),
  serial_fd_(-1),
  running_(false),
  emergency_stopped_(false),
  last_odom_time_(this->now()),
  last_cmd_vel_time_(this->now())
{
  RCLCPP_INFO(this->get_logger(), "Initializing Serial Bridge Node...");

  // 파라미터 선언
  declareParameters();

  // Odometry Calculator 초기화
  odom_calc_ = std::make_unique<OdometryCalculator>(
    wheel_base_,
    wheel_radius_,
    encoder_ticks_per_rev_
  );

  // Publishers
  imu_pub_ = this->create_publisher<sensor_msgs::msg::Imu>(
    "/imu/data", 10);
  
  wheel_odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>(
    "/wheel_odom", 10);

  // Subscribers
  cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
    "/cmd_vel", 10,
    std::bind(&SerialBridgeNode::cmdVelCallback, this, std::placeholders::_1));

  emergency_stop_sub_ = this->create_subscription<std_msgs::msg::Bool>(
    "/emergency_stop", 10,
    std::bind(&SerialBridgeNode::emergencyStopCallback, this, std::placeholders::_1));

  // TF Broadcaster
  tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

  // Timer for cmd_vel repeat (타임아웃 방지)
  auto repeat_duration = std::chrono::duration<double>(1.0 / cmd_vel_repeat_rate_);
  cmd_vel_repeat_timer_ = this->create_wall_timer(
    repeat_duration,
    std::bind(&SerialBridgeNode::repeatCmdVelTimer, this));

  // Serial Protocol 콜백 등록
  protocol_.setOdomCallback(
    std::bind(&SerialBridgeNode::handleOdomData, this, 
              std::placeholders::_1, std::placeholders::_2));
  
  protocol_.setImuCallback(
    std::bind(&SerialBridgeNode::handleImuData, this, std::placeholders::_1));

  // 시리얼 포트 열기
  if (!openSerialPort()) {
    RCLCPP_ERROR(this->get_logger(), "Failed to open serial port!");
    return;
  }

  // 시리얼 읽기 스레드 시작
  running_ = true;
  serial_read_thread_ = std::thread(&SerialBridgeNode::serialReadLoop, this);

  // STM32 커맨드 로그 토픽 (/serial_bridge/cmd_log, std_msgs/Bool)
  cmd_log_sub_ = this->create_subscription<std_msgs::msg::Bool>(
    "/serial_bridge/cmd_log", 10,
    std::bind(&SerialBridgeNode::cmdLogCallback, this, std::placeholders::_1));

  RCLCPP_INFO(this->get_logger(), "Serial Bridge Node initialized successfully");
}

SerialBridgeNode::~SerialBridgeNode()
{
  RCLCPP_INFO(this->get_logger(), "Shutting down Serial Bridge Node...");
  
  // 스레드 종료
  running_ = false;
  if (serial_read_thread_.joinable()) {
    serial_read_thread_.join();
  }

  // 시리얼 포트 닫기
  closeSerialPort();

  // 로그 파일 닫기
  if (cmd_log_file_.is_open()) {
    cmd_log_file_.close();
  }
}

void SerialBridgeNode::cmdLogCallback(const std_msgs::msg::Bool::SharedPtr msg)
{
  if (msg->data) {
    // 로그 시작
    if (cmd_log_file_.is_open()) {
      RCLCPP_WARN(this->get_logger(), "Already logging");
      return;
    }
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
    std::string log_path = std::string("/root/ros2_ws/log/cmd_stm32_") + buf + ".csv";
    cmd_log_file_.open(log_path, std::ios::out);
    if (cmd_log_file_.is_open()) {
      cmd_log_file_ << "timestamp,linear_x,angular_z,left_mm_s,right_mm_s\n";

      RCLCPP_INFO(this->get_logger(), "STM32 command log started: %s", log_path.c_str());
    } else {

    }
  } else {
    // 로그 정지
    if (cmd_log_file_.is_open()) {
      cmd_log_file_.close();
      RCLCPP_INFO(this->get_logger(), "STM32 command log stopped");
    }

  }
}

void SerialBridgeNode::declareParameters()
{
  // 시리얼 설정
  this->declare_parameter("serial_port", "/dev/ttyAMA0");
  this->declare_parameter("baud_rate", 115200);

  // 로봇 물리 파라미터
  this->declare_parameter("wheel_base", 0.16);
  this->declare_parameter("wheel_radius", 0.033);
  this->declare_parameter("encoder_ticks_per_rev", 20);

  // IMU 스케일
  this->declare_parameter("imu_accel_scale", 0.005875);   // m/s² per LSB
  this->declare_parameter("imu_gyro_scale", 0.0001332);   // rad/s per LSB

  // 좌표계 변환
  this->declare_parameter("imu_transform_enabled", false);

  // 프레임 ID
  this->declare_parameter("odom_frame_id", "odom");
  this->declare_parameter("base_frame_id", "base_footprint");
  this->declare_parameter("imu_frame_id", "imu_link");

  // 타이밍
  this->declare_parameter("cmd_vel_repeat_rate", 5.0);
  this->declare_parameter("min_angular_vel", 0.4);

  // 오도메트리 타입
  this->declare_parameter("odom_data_type", "ticks");  // "ticks" or "velocity"

  // 파라미터 읽기
  serial_port_ = this->get_parameter("serial_port").as_string();
  baud_rate_ = this->get_parameter("baud_rate").as_int();
  wheel_base_ = this->get_parameter("wheel_base").as_double();
  wheel_radius_ = this->get_parameter("wheel_radius").as_double();
  encoder_ticks_per_rev_ = this->get_parameter("encoder_ticks_per_rev").as_int();
  imu_accel_scale_ = this->get_parameter("imu_accel_scale").as_double();
  imu_gyro_scale_ = this->get_parameter("imu_gyro_scale").as_double();
  imu_transform_enabled_ = this->get_parameter("imu_transform_enabled").as_bool();
  odom_frame_id_ = this->get_parameter("odom_frame_id").as_string();
  base_frame_id_ = this->get_parameter("base_frame_id").as_string();
  imu_frame_id_ = this->get_parameter("imu_frame_id").as_string();
  cmd_vel_repeat_rate_ = this->get_parameter("cmd_vel_repeat_rate").as_double();
  min_angular_vel_ = this->get_parameter("min_angular_vel").as_double();
  odom_data_type_ = this->get_parameter("odom_data_type").as_string();

  RCLCPP_INFO(this->get_logger(), "Parameters loaded:");
  RCLCPP_INFO(this->get_logger(), "  serial_port: %s", serial_port_.c_str());
  RCLCPP_INFO(this->get_logger(), "  baud_rate: %d", baud_rate_);
  RCLCPP_INFO(this->get_logger(), "  wheel_base: %.3f m", wheel_base_);
  RCLCPP_INFO(this->get_logger(), "  wheel_radius: %.4f m", wheel_radius_);
  RCLCPP_INFO(this->get_logger(), "  encoder_ticks_per_rev: %d  (meters_per_tick=%.7f)",
              encoder_ticks_per_rev_,
              (2.0 * M_PI * wheel_radius_) / encoder_ticks_per_rev_);
  RCLCPP_INFO(this->get_logger(), "  odom_data_type: %s", odom_data_type_.c_str());
}

bool SerialBridgeNode::openSerialPort()
{
  RCLCPP_INFO(this->get_logger(), "Opening serial port: %s @ %d baud",
              serial_port_.c_str(), baud_rate_);

  serial_fd_ = open(serial_port_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
  if (serial_fd_ == -1) {
    RCLCPP_ERROR(this->get_logger(), "Failed to open port: %s", strerror(errno));
    return false;
  }

  // 시리얼 포트 설정
  struct termios options;
  if (tcgetattr(serial_fd_, &options) != 0) {
    RCLCPP_ERROR(this->get_logger(), "Failed to get port attributes");
    close(serial_fd_);
    serial_fd_ = -1;
    return false;
  }

  // Baud rate 설정
  speed_t baud;
  switch (baud_rate_) {
    case 9600:   baud = B9600; break;
    case 19200:  baud = B19200; break;
    case 38400:  baud = B38400; break;
    case 57600:  baud = B57600; break;
    case 115200: baud = B115200; break;
    default:
      RCLCPP_ERROR(this->get_logger(), "Unsupported baud rate: %d", baud_rate_);
      close(serial_fd_);
      serial_fd_ = -1;
      return false;
  }

  cfsetispeed(&options, baud);
  cfsetospeed(&options, baud);

  // 8N1 설정
  options.c_cflag &= ~PARENB;   // No parity
  options.c_cflag &= ~CSTOPB;   // 1 stop bit
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;       // 8 data bits
  options.c_cflag |= CREAD | CLOCAL;

  // Raw mode
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_iflag &= ~(IXON | IXOFF | IXANY);
  options.c_oflag &= ~OPOST;

  // Read timeout
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 1;  // 0.1초 타임아웃

  if (tcsetattr(serial_fd_, TCSANOW, &options) != 0) {
    RCLCPP_ERROR(this->get_logger(), "Failed to set port attributes");
    close(serial_fd_);
    serial_fd_ = -1;
    return false;
  }

  // Flush
  tcflush(serial_fd_, TCIOFLUSH);

  RCLCPP_INFO(this->get_logger(), "Serial port opened successfully");
  return true;
}

void SerialBridgeNode::closeSerialPort()
{
  if (serial_fd_ != -1) {
    close(serial_fd_);
    serial_fd_ = -1;
    RCLCPP_INFO(this->get_logger(), "Serial port closed");
  }
}

void SerialBridgeNode::serialReadLoop()
{
  uint8_t buffer[256];
  
  while (running_) {
    if (serial_fd_ == -1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    int bytes_read = read(serial_fd_, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
      for (int i = 0; i < bytes_read; i++) {
        protocol_.processByte(buffer[i]);
      }
    } else if (bytes_read < 0 && errno != EAGAIN) {
      RCLCPP_WARN(this->get_logger(), "Serial read error: %s", strerror(errno));
    }

    // 짧은 sleep으로 CPU 사용률 낮춤
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}

void SerialBridgeNode::cmdVelCallback(const geometry_msgs::msg::Twist::SharedPtr msg)
{
  if (emergency_stopped_) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                         "Emergency stopped - ignoring cmd_vel");
    return;
  }

  double linear_x = msg->linear.x;
  double angular_z = msg->angular.z;

  // angular.z 최소값 강제 적용 (정지 마찰 극복)
  if (std::abs(angular_z) > 1e-3 && std::abs(angular_z) < min_angular_vel_) {
    angular_z = std::copysign(min_angular_vel_, angular_z);
  }

  last_cmd_vel_ = *msg;
  last_cmd_vel_.angular.z = angular_z;
  last_cmd_vel_time_ = this->now();

  sendVelocityCommand(linear_x, angular_z);
}

void SerialBridgeNode::emergencyStopCallback(const std_msgs::msg::Bool::SharedPtr msg)
{
  emergency_stopped_ = msg->data;

  if (msg->data) {
    RCLCPP_WARN(this->get_logger(), "EMERGENCY STOP activated!");
    auto packet = protocol_.createEmergencyStopPacket();
    if (serial_fd_ != -1) {
      write(serial_fd_, packet.data(), packet.size());
    }
  } else {
    RCLCPP_INFO(this->get_logger(), "Emergency stop released");
    auto packet = protocol_.createEmergencyReleasePacket();
    if (serial_fd_ != -1) {
      write(serial_fd_, packet.data(), packet.size());
    }
  }
}

void SerialBridgeNode::repeatCmdVelTimer()
{
  // 타임아웃 방지: 마지막 cmd_vel 재전송 (STM32가 500ms 타임아웃)
  if (emergency_stopped_) {
    return;
  }

  // 최근 1초 이내에 cmd_vel을 받았으면 재전송
  auto now = this->now();
  auto time_since_last_cmd = (now - last_cmd_vel_time_).seconds();
  
  if (time_since_last_cmd < 1.0) {
    sendVelocityCommand(last_cmd_vel_.linear.x, last_cmd_vel_.angular.z);
  }
}

void SerialBridgeNode::sendVelocityCommand(double linear_x, double angular_z)
{
  if (serial_fd_ == -1) {
    RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                          "Serial port not opened!");
    return;
  }

  // Twist → 좌/우 바퀴 속도 (m/s)
  
  const double ANGULAR_GAIN = (std::abs(linear_x) < 1e-3) ? 6.8 : 3.8;
				   
  double angular_scaled = angular_z * ANGULAR_GAIN;
  
  auto [v_left, v_right] = odom_calc_->twistToWheelVelocities(linear_x, angular_scaled);

  //auto [v_left, v_right] = odom_calc_->twistToWheelVelocities(linear_x, angular_z);

  // m/s → mm/s 변환 (int16_t)
  int16_t left_mm_s = static_cast<int16_t>(v_left * 1000.0);
  int16_t right_mm_s = static_cast<int16_t>(v_right * 1000.0);

  // 속도 제한 확인 (오버플로우 방지)
  const int16_t MAX_SPEED = 1000;  // 1 m/s = 1000 mm/s
  left_mm_s = std::clamp(left_mm_s, static_cast<int16_t>(-MAX_SPEED), MAX_SPEED);
  right_mm_s = std::clamp(right_mm_s, static_cast<int16_t>(-MAX_SPEED), MAX_SPEED);

  // 패킷 생성 및 전송
  auto packet = protocol_.createVelocityPacket(left_mm_s, right_mm_s);
  
  // 디버그: 패킷 내용 출력
  std::stringstream ss;
  ss << "Sending packet (" << packet.size() << " bytes): ";
  for (auto byte : packet) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
  }
  RCLCPP_DEBUG(this->get_logger(), "%s", ss.str().c_str());
  
  // STM32 커맨드 로그 기록
  if (cmd_log_file_.is_open()) {
    double ts = this->now().seconds();
    cmd_log_file_ << std::fixed << std::setprecision(6)
                  << ts << ","
                  << linear_x << ","
                  << angular_z << ","
                  << left_mm_s << ","
                  << right_mm_s << "\n";
  }

  ssize_t written = write(serial_fd_, packet.data(), packet.size());

  if (written < 0) {
    RCLCPP_ERROR_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                          "Write error: %s (errno=%d)", strerror(errno), errno);
  } else if (written != static_cast<ssize_t>(packet.size())) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                         "Partial write: %zd/%zu bytes (Left=%d, Right=%d mm/s)",
                         written, packet.size(), left_mm_s, right_mm_s);;
  }
}

void SerialBridgeNode::handleOdomData(int16_t left, int16_t right)
{
  auto now = this->now();
  double dt = (now - last_odom_time_).seconds();
  last_odom_time_ = now;

  // 첫 호출 시에는 dt가 매우 작으므로 스킵
  if (dt < 0.001 || dt > 1.0) {
    return;
  }

  // 데이터 타입에 따라 처리
  if (odom_data_type_ == "ticks") {
    // 엔코더 틱 카운트
    odom_calc_->updateFromTicks(left, right, dt);
  } else if (odom_data_type_ == "velocity") {
    // 속도 (mm/s)
    odom_calc_->updateFromVelocities(left, right, dt);
  }

  // Publish
  publishWheelOdom();
}

void SerialBridgeNode::handleImuData(const int16_t imu_raw[6])
{
  publishImu(imu_raw);
}

void SerialBridgeNode::publishImu(const int16_t imu_raw[6])
{
  sensor_msgs::msg::Imu imu_msg;
  
  imu_msg.header.stamp = this->now();
  imu_msg.header.frame_id = imu_frame_id_;

  // 좌표계 변환 (필요시)
  int16_t transformed[6];
  std::copy(imu_raw, imu_raw + 6, transformed);
  
  if (imu_transform_enabled_) {
    transformImuData(transformed);
  }

  // 스케일 적용 및 단위 변환
  // Acceleration (m/s²)
  imu_msg.linear_acceleration.x = transformed[0] * imu_accel_scale_;
  imu_msg.linear_acceleration.y = transformed[1] * imu_accel_scale_;
  imu_msg.linear_acceleration.z = transformed[2] * imu_accel_scale_;

  // Angular velocity (rad/s)
  imu_msg.angular_velocity.x = transformed[3] * imu_gyro_scale_;
  imu_msg.angular_velocity.y = transformed[4] * imu_gyro_scale_;
  imu_msg.angular_velocity.z = transformed[5] * imu_gyro_scale_;

  // Orientation은 없음 (IMU만으로는 계산 불가)
  imu_msg.orientation_covariance[0] = -1.0;

  // Covariance (임시 값, 나중에 캘리브레이션 필요)
  imu_msg.linear_acceleration_covariance[0] = 0.01;
  imu_msg.linear_acceleration_covariance[4] = 0.01;
  imu_msg.linear_acceleration_covariance[8] = 0.01;

  imu_msg.angular_velocity_covariance[0] = 0.001;
  imu_msg.angular_velocity_covariance[4] = 0.001;
  imu_msg.angular_velocity_covariance[8] = 0.001;

  imu_pub_->publish(imu_msg);
}

void SerialBridgeNode::publishWheelOdom()
{
  auto now = this->now();
  auto odom_msg = odom_calc_->createOdometryMsg(
    odom_frame_id_,
    base_frame_id_,
    now
  );

  wheel_odom_pub_->publish(odom_msg);
  // TF는 EKF(ekf_filter_node)가 발행 → 여기서 중복 발행 금지
}

void SerialBridgeNode::publishTransform(const nav_msgs::msg::Odometry& odom)
{
  geometry_msgs::msg::TransformStamped transform;
  
  transform.header.stamp = odom.header.stamp;
  transform.header.frame_id = odom.header.frame_id;
  transform.child_frame_id = odom.child_frame_id;
  
  transform.transform.translation.x = odom.pose.pose.position.x;
  transform.transform.translation.y = odom.pose.pose.position.y;
  transform.transform.translation.z = odom.pose.pose.position.z;
  transform.transform.rotation = odom.pose.pose.orientation;
  
  tf_broadcaster_->sendTransform(transform);
}

void SerialBridgeNode::transformImuData(int16_t imu_raw[6]) const
{
  // TODO: MPU6050 좌표계 → ROS2 좌표계 (REP-103) 변환
  // 실제 장착 방향에 따라 변환 행렬 적용
  // 현재는 변환 없음 (identity)
  
  // 예시: 90도 회전이 필요한 경우
  // int16_t ax = imu_raw[0];
  // int16_t ay = imu_raw[1];
  // imu_raw[0] = ay;
  // imu_raw[1] = -ax;
}

}  // namespace rpi_serial_bridge
