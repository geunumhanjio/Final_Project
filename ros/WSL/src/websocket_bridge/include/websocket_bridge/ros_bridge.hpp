#ifndef WEBSOCKET_BRIDGE__ROS_BRIDGE_HPP_
#define WEBSOCKET_BRIDGE__ROS_BRIDGE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/path.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float32.hpp>
#include <json/json.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <memory>
#include <functional>
#include <mutex>

class ROSBridge : public rclcpp::Node
{
public:
    using BroadcastCallback = std::function<void(const Json::Value&)>;

    ROSBridge();
    void setBroadcastCallback(BroadcastCallback callback);
    void processClientMessage(const Json::Value& message);

private:
    // Client → ROS2 처리
    void handleNavCommand(const Json::Value& data);
    void handleCmdVel(const Json::Value& data);
    void handleModeControl(const Json::Value& data);
    void handleEmergencyStop(const Json::Value& data);

    // ROS2 → Client 콜백
    void odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg);
    void mapCallback(const nav_msgs::msg::OccupancyGrid::SharedPtr msg);
    void pathCallback(const nav_msgs::msg::Path::SharedPtr msg);
    void navStatusCallback(const std_msgs::msg::String::SharedPtr msg);
    void navFeedbackCallback(const std_msgs::msg::String::SharedPtr msg);
    void trackingStatusCallback(const std_msgs::msg::String::SharedPtr msg);

    // odom 타이머 콜백 (odom_publish_mode == "timer")
    void odomTimerCallback();

    // 유틸리티
    double quaternionToYaw(const geometry_msgs::msg::Quaternion& quat);
    Json::Value createTimestampedMessage(const std::string& type);
    void broadcastOdom(double x, double y, double theta, double vx, double vz);

    // Publishers (Client → ROS2)
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr nav_cmd_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr mode_pub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr estop_pub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr tracking_enable_pub_;
    rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr tilt_pub_;

    // Subscribers (ROS2 → Client)
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_sub_;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr path_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr nav_status_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr nav_feedback_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr tracking_status_sub_;

    // WebSocket 브로드캐스트 콜백
    BroadcastCallback broadcast_callback_;

    // ── 파라미터 ─────────────────────────────────────────────────────────────
    // odom_source       : "slam" → map→base_footprint TF 사용 (SLAM 기준 위치)
    //                     "odom" → /odom 토픽 사용 (EKF 오도메트리 기준 위치)
    // odom_publish_mode : "timer" → 고정 주기로 전송 (odom_publish_interval)
    //                     "topic" → /odom 메시지 도착 시 전송 (odom_publish_interval로 다운샘플)
    // odom_publish_interval : 전송 주기(초), timer=고정주기, topic=최소간격
    std::string odom_source_;
    std::string odom_publish_mode_;
    double odom_publish_interval_;

    // TF2 (slam 소스 전용)
    std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
    std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

    // odom 타이머 (timer 모드)
    rclcpp::TimerBase::SharedPtr odom_timer_;

    // 최신 odom 캐시 (속도 정보 + odom 소스 + topic 모드용)
    nav_msgs::msg::Odometry::SharedPtr latest_odom_;
    std::mutex odom_mutex_;

    // 다운샘플링용 (topic 모드)
    rclcpp::Time last_odom_time_;
};

#endif  // WEBSOCKET_BRIDGE__ROS_BRIDGE_HPP_
