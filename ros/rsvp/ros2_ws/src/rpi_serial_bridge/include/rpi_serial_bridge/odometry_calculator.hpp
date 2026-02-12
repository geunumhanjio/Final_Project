#ifndef RPI_SERIAL_BRIDGE__ODOMETRY_CALCULATOR_HPP_
#define RPI_SERIAL_BRIDGE__ODOMETRY_CALCULATOR_HPP_

#include <cstdint>
#include <utility>
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/pose2_d.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"

namespace rpi_serial_bridge
{

/**
 * @brief 차동 구동 로봇의 오도메트리 계산 클래스
 */
class OdometryCalculator
{
public:
  /**
   * @brief 생성자
   * @param wheel_base 바퀴 간격 (m)
   * @param wheel_radius 바퀴 반경 (m)
   * @param ticks_per_rev 엔코더 틱/회전
   */
  OdometryCalculator(
    double wheel_base,
    double wheel_radius,
    int ticks_per_rev);

  /**
   * @brief cmd_vel → 좌/우 바퀴 속도 변환
   * @param linear_x 전진 속도 (m/s)
   * @param angular_z 회전 속도 (rad/s)
   * @return {left_m_s, right_m_s}
   */
  std::pair<double, double> twistToWheelVelocities(
    double linear_x,
    double angular_z) const;

  /**
   * @brief 엔코더 틱 카운트로부터 오도메트리 업데이트
   * @param left_ticks 좌측 엔코더 틱 (누적)
   * @param right_ticks 우측 엔코더 틱 (누적)
   * @param dt 시간 간격 (s)
   */
  void updateFromTicks(int16_t left_ticks, int16_t right_ticks, double dt);

  /**
   * @brief 바퀴 속도로부터 오도메트리 업데이트
   * @param left_velocity 좌측 바퀴 속도 (mm/s)
   * @param right_velocity 우측 바퀴 속도 (mm/s)
   * @param dt 시간 간격 (s)
   */
  void updateFromVelocities(int16_t left_velocity, int16_t right_velocity, double dt);

  /**
   * @brief 현재 pose 반환
   */
  geometry_msgs::msg::Pose2D getPose() const;

  /**
   * @brief 현재 twist 반환
   */
  geometry_msgs::msg::Twist getTwist() const;

  /**
   * @brief Odometry 메시지 생성
   */
  nav_msgs::msg::Odometry createOdometryMsg(
    const std::string& frame_id,
    const std::string& child_frame_id,
    const rclcpp::Time& timestamp) const;

  /**
   * @brief 오도메트리 리셋
   */
  void reset();

private:
  // 로봇 파라미터
  double wheel_base_;       // m
  double wheel_radius_;     // m
  int ticks_per_rev_;       // ticks/revolution
  double meters_per_tick_;  // m/tick

  // 상태 변수
  double x_;       // m
  double y_;       // m
  double theta_;   // rad
  double vx_;      // m/s
  double vy_;      // m/s (항상 0, 차동구동)
  double vth_;     // rad/s

  // 이전 틱 값 (오버플로우 처리용)
  int32_t prev_left_ticks_;
  int32_t prev_right_ticks_;

  /**
   * @brief int16_t 오버플로우를 고려한 차분 계산
   */
  int32_t calculateTickDelta(int16_t current, int16_t previous) const;
};

}  // namespace rpi_serial_bridge

#endif  // RPI_SERIAL_BRIDGE__ODOMETRY_CALCULATOR_HPP_
