#ifndef RPI_SERIAL_BRIDGE__SERIAL_BRIDGE_NODE_HPP_
#define RPI_SERIAL_BRIDGE__SERIAL_BRIDGE_NODE_HPP_

#include <memory>
#include <string>
#include <thread>
#include <atomic>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "std_msgs/msg/bool.hpp"
#include "tf2_ros/transform_broadcaster.h"

#include "rpi_serial_bridge/serial_protocol.hpp"
#include "rpi_serial_bridge/odometry_calculator.hpp"

namespace rpi_serial_bridge
{

class SerialBridgeNode : public rclcpp::Node
{
public:
  explicit SerialBridgeNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  ~SerialBridgeNode();

private:
  // === ROS2 Publishers ===
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr imu_pub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr wheel_odom_pub_;

  // === ROS2 Subscribers ===
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr emergency_stop_sub_;

  // === TF Broadcaster ===
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  // === Timers ===
  rclcpp::TimerBase::SharedPtr cmd_vel_repeat_timer_;

  // === Serial Communication ===
  int serial_fd_;
  std::thread serial_read_thread_;
  std::atomic<bool> running_;
  SerialProtocol protocol_;

  // === Odometry ===
  std::unique_ptr<OdometryCalculator> odom_calc_;
  rclcpp::Time last_odom_time_;

  // === Parameters ===
  std::string serial_port_;
  int baud_rate_;
  double wheel_base_;
  double wheel_radius_;
  int encoder_ticks_per_rev_;
  double imu_accel_scale_;
  double imu_gyro_scale_;
  bool imu_transform_enabled_;
  std::string odom_frame_id_;
  std::string base_frame_id_;
  std::string imu_frame_id_;
  double cmd_vel_repeat_rate_;
  double min_angular_vel_;
  std::string odom_data_type_;

  // === State ===
  geometry_msgs::msg::Twist last_cmd_vel_;
  bool emergency_stopped_;
  rclcpp::Time last_cmd_vel_time_;

  // === Methods ===
  void declareParameters();
  bool openSerialPort();
  void closeSerialPort();
  void serialReadLoop();

  // === ROS2 Callbacks ===
  void cmdVelCallback(const geometry_msgs::msg::Twist::SharedPtr msg);
  void emergencyStopCallback(const std_msgs::msg::Bool::SharedPtr msg);
  void repeatCmdVelTimer();

  // === Serial Protocol Callbacks ===
  void handleOdomData(int16_t left, int16_t right);
  void handleImuData(const int16_t imu_raw[6]);

  // === Publishers ===
  void publishImu(const int16_t imu_raw[6]);
  void publishWheelOdom();
  void publishTransform(const nav_msgs::msg::Odometry& odom);

  // === Utilities ===
  void sendVelocityCommand(double linear_x, double angular_z);
  void transformImuData(int16_t imu_raw[6]) const;
};

}  // namespace rpi_serial_bridge

#endif  // RPI_SERIAL_BRIDGE__SERIAL_BRIDGE_NODE_HPP_
