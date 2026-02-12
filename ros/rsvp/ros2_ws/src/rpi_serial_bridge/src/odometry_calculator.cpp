#include "rpi_serial_bridge/odometry_calculator.hpp"
#include <cmath>
#include "tf2/LinearMath/Quaternion.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.hpp"

namespace rpi_serial_bridge
{

OdometryCalculator::OdometryCalculator(
  double wheel_base,
  double wheel_radius,
  int ticks_per_rev)
: wheel_base_(wheel_base),
  wheel_radius_(wheel_radius),
  ticks_per_rev_(ticks_per_rev),
  x_(0.0),
  y_(0.0),
  theta_(0.0),
  vx_(0.0),
  vy_(0.0),
  vth_(0.0),
  prev_left_ticks_(0),
  prev_right_ticks_(0)
{
  // 1 틱당 이동 거리 계산
  meters_per_tick_ = (2.0 * M_PI * wheel_radius_) / ticks_per_rev_;
}

std::pair<double, double> OdometryCalculator::twistToWheelVelocities(
  double linear_x,
  double angular_z) const
{
  // 차동 구동 역기구학
  // v_left  = linear_x - (angular_z * wheel_base / 2)
  // v_right = linear_x + (angular_z * wheel_base / 2)
  
  double v_left = linear_x - (angular_z * wheel_base_ / 2.0);
  double v_right = linear_x + (angular_z * wheel_base_ / 2.0);
  
  return {v_left, v_right};
}

int32_t OdometryCalculator::calculateTickDelta(int16_t current, int16_t previous) const
{
  // int16_t 오버플로우 처리
  int32_t delta = static_cast<int32_t>(current) - static_cast<int32_t>(previous);
  
  // -32768 ~ 32767 범위에서 오버플로우 감지
  if (delta > 32767) {
    delta -= 65536;
  } else if (delta < -32768) {
    delta += 65536;
  }
  
  return delta;
}

void OdometryCalculator::updateFromTicks(int16_t left_ticks, int16_t right_ticks, double dt)
{
  // 틱 차분 계산 (오버플로우 고려)
  int32_t delta_left = calculateTickDelta(left_ticks, prev_left_ticks_);
  int32_t delta_right = calculateTickDelta(right_ticks, prev_right_ticks_);
  
  // 이전 값 저장
  prev_left_ticks_ = left_ticks;
  prev_right_ticks_ = right_ticks;
  
  // 거리 계산
  double distance_left = delta_left * meters_per_tick_;
  double distance_right = delta_right * meters_per_tick_;
  
  // 중심 거리 및 각도 변화
  double distance_center = (distance_left + distance_right) / 2.0;
  double delta_theta = (distance_right - distance_left) / wheel_base_;
  
  // 속도 계산
  if (dt > 0.0) {
    vx_ = distance_center / dt;
    vth_ = delta_theta / dt;
  } else {
    vx_ = 0.0;
    vth_ = 0.0;
  }
  vy_ = 0.0;  // 차동 구동은 횡방향 이동 없음
  
  // 위치 업데이트 (오일러 적분)
  double delta_x = distance_center * cos(theta_ + delta_theta / 2.0);
  double delta_y = distance_center * sin(theta_ + delta_theta / 2.0);
  
  x_ += delta_x;
  y_ += delta_y;
  theta_ += delta_theta;
  
  // 각도 정규화 (-π ~ π)
  while (theta_ > M_PI) theta_ -= 2.0 * M_PI;
  while (theta_ < -M_PI) theta_ += 2.0 * M_PI;
}

void OdometryCalculator::updateFromVelocities(
  int16_t left_velocity,
  int16_t right_velocity,
  double dt)
{
  // mm/s → m/s 변환
  double v_left = left_velocity / 1000.0;
  double v_right = right_velocity / 1000.0;
  
  // 중심 속도 및 각속도
  vx_ = (v_left + v_right) / 2.0;
  vth_ = (v_right - v_left) / wheel_base_;
  vy_ = 0.0;
  
  // 위치 업데이트
  if (dt > 0.0) {
    double delta_s = vx_ * dt;
    double delta_theta = vth_ * dt;
    
    double delta_x = delta_s * cos(theta_ + delta_theta / 2.0);
    double delta_y = delta_s * sin(theta_ + delta_theta / 2.0);
    
    x_ += delta_x;
    y_ += delta_y;
    theta_ += delta_theta;
    
    // 각도 정규화
    while (theta_ > M_PI) theta_ -= 2.0 * M_PI;
    while (theta_ < -M_PI) theta_ += 2.0 * M_PI;
  }
}

geometry_msgs::msg::Pose2D OdometryCalculator::getPose() const
{
  geometry_msgs::msg::Pose2D pose;
  pose.x = x_;
  pose.y = y_;
  pose.theta = theta_;
  return pose;
}

geometry_msgs::msg::Twist OdometryCalculator::getTwist() const
{
  geometry_msgs::msg::Twist twist;
  twist.linear.x = vx_;
  twist.linear.y = vy_;
  twist.linear.z = 0.0;
  twist.angular.x = 0.0;
  twist.angular.y = 0.0;
  twist.angular.z = vth_;
  return twist;
}

nav_msgs::msg::Odometry OdometryCalculator::createOdometryMsg(
  const std::string& frame_id,
  const std::string& child_frame_id,
  const rclcpp::Time& timestamp) const
{
  nav_msgs::msg::Odometry odom;
  
  odom.header.stamp = timestamp;
  odom.header.frame_id = frame_id;
  odom.child_frame_id = child_frame_id;
  
  // Pose
  odom.pose.pose.position.x = x_;
  odom.pose.pose.position.y = y_;
  odom.pose.pose.position.z = 0.0;
  
  tf2::Quaternion q;
  q.setRPY(0, 0, theta_);
  odom.pose.pose.orientation = tf2::toMsg(q);
  
  // Twist
  odom.twist.twist = getTwist();
  
  // Covariance (임시 값, 나중에 튜닝 필요)
  odom.pose.covariance[0] = 0.001;   // x
  odom.pose.covariance[7] = 0.001;   // y
  odom.pose.covariance[35] = 0.01;   // yaw
  
  odom.twist.covariance[0] = 0.001;  // vx
  odom.twist.covariance[35] = 0.01;  // vyaw
  
  return odom;
}

void OdometryCalculator::reset()
{
  x_ = 0.0;
  y_ = 0.0;
  theta_ = 0.0;
  vx_ = 0.0;
  vy_ = 0.0;
  vth_ = 0.0;
  prev_left_ticks_ = 0;
  prev_right_ticks_ = 0;
}

}  // namespace rpi_serial_bridge
