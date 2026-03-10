#ifndef WEBSOCKET_BRIDGE__ROS_BRIDGE_HPP_
#define WEBSOCKET_BRIDGE__ROS_BRIDGE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/path.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/bool.hpp>
#include <json/json.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <memory>
#include <functional>

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

    // 유틸리티
    double quaternionToYaw(const geometry_msgs::msg::Quaternion& quat);
    Json::Value createTimestampedMessage(const std::string& type);

    // Publishers (Client → ROS2)
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr nav_cmd_pub_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr mode_pub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr estop_pub_;

    // Subscribers (ROS2 → Client)
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_sub_;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr path_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr nav_status_sub_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr nav_feedback_sub_;

    // WebSocket 브로드캐스트 콜백
    BroadcastCallback broadcast_callback_;

    // 다운샘플링용 타이머
    rclcpp::Time last_odom_time_;
    rclcpp::Time last_map_time_;
};

#endif  // WEBSOCKET_BRIDGE__ROS_BRIDGE_HPP_
