#include "websocket_bridge/ros_bridge.hpp"
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

ROSBridge::ROSBridge()
    : Node("ros_bridge_node")
{
    // Publishers
    goal_pub_ = this->create_publisher<geometry_msgs::msg::PoseStamped>(
        "/goal_pose", 10);
    
    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>(
        "/cmd_vel", 10);
    
    mode_pub_ = this->create_publisher<std_msgs::msg::String>(
        "/mode_control", 10);
    
    estop_pub_ = this->create_publisher<std_msgs::msg::Bool>(
        "/emergency_stop", 10);

    // Subscribers
    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
        "/odom", 10,
        std::bind(&ROSBridge::odomCallback, this, std::placeholders::_1));

    map_sub_ = this->create_subscription<nav_msgs::msg::OccupancyGrid>(
        "/map", rclcpp::QoS(rclcpp::KeepLast(1)).transient_local().reliable(),
        std::bind(&ROSBridge::mapCallback, this, std::placeholders::_1));

    path_sub_ = this->create_subscription<nav_msgs::msg::Path>(
        "/plan", 10,
        std::bind(&ROSBridge::pathCallback, this, std::placeholders::_1));

    status_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/robot_status", 10,
        std::bind(&ROSBridge::statusCallback, this, std::placeholders::_1));

    last_odom_time_ = this->now();
    last_map_time_ = this->now();

    RCLCPP_INFO(this->get_logger(), "ROS Bridge initialized");
}

void ROSBridge::setBroadcastCallback(BroadcastCallback callback)
{
    broadcast_callback_ = callback;
}

void ROSBridge::processClientMessage(const Json::Value& message)
{
    std::string type = message["type"].asString();
    Json::Value data = message["data"];

    if (type == "goal_pose") {
        handleGoalPose(data);
    } else if (type == "cmd_vel") {
        handleCmdVel(data);
    } else if (type == "mode_control") {
        handleModeControl(data);
    } else if (type == "emergency_stop") {
        handleEmergencyStop(data);
    } else {
        RCLCPP_WARN(this->get_logger(), "Unknown message type: %s", type.c_str());
    }
}

void ROSBridge::handleGoalPose(const Json::Value& data)
{
    auto pose_msg = geometry_msgs::msg::PoseStamped();
    pose_msg.header.stamp = this->now();
    pose_msg.header.frame_id = data.get("frame_id", "map").asString();

    pose_msg.pose.position.x = data["x"].asDouble();
    pose_msg.pose.position.y = data["y"].asDouble();
    pose_msg.pose.position.z = 0.0;

    // theta → quaternion
    double theta = data.get("theta", 0.0).asDouble();
    tf2::Quaternion q;
    q.setRPY(0, 0, theta);
    pose_msg.pose.orientation = tf2::toMsg(q);

    goal_pub_->publish(pose_msg);
    
    RCLCPP_INFO(this->get_logger(), 
                "Goal pose published: (%.2f, %.2f, %.2f)", 
                pose_msg.pose.position.x, 
                pose_msg.pose.position.y, 
                theta);
}

void ROSBridge::handleCmdVel(const Json::Value& data)
{
    auto twist_msg = geometry_msgs::msg::Twist();
    twist_msg.linear.x = data.get("linear_x", 0.0).asDouble();
    twist_msg.linear.y = data.get("linear_y", 0.0).asDouble();
    twist_msg.angular.z = data.get("angular_z", 0.0).asDouble();

    cmd_vel_pub_->publish(twist_msg);
}

void ROSBridge::handleModeControl(const Json::Value& data)
{
    auto mode_msg = std_msgs::msg::String();
    mode_msg.data = data.get("mode", "manual").asString();

    mode_pub_->publish(mode_msg);
    
    RCLCPP_INFO(this->get_logger(), "Mode changed to: %s", mode_msg.data.c_str());
}

void ROSBridge::handleEmergencyStop(const Json::Value& data)
{
    auto estop_msg = std_msgs::msg::Bool();
    estop_msg.data = data.get("stop", true).asBool();

    estop_pub_->publish(estop_msg);

    if (estop_msg.data) {
        // 속도도 0으로
        auto twist_msg = geometry_msgs::msg::Twist();
        cmd_vel_pub_->publish(twist_msg);
        
        RCLCPP_WARN(this->get_logger(), "EMERGENCY STOP ACTIVATED!");
    }
}

void ROSBridge::odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg)
{
    // 다운샘플링: 100ms마다 (10Hz)
    auto now = this->now();
    if ((now - last_odom_time_).seconds() < 0.1) {
        return;
    }
    last_odom_time_ = now;

    if (!broadcast_callback_) return;

    double theta = quaternionToYaw(msg->pose.pose.orientation);

    Json::Value ws_msg = createTimestampedMessage("odom");
    ws_msg["data"]["position"]["x"] = msg->pose.pose.position.x;
    ws_msg["data"]["position"]["y"] = msg->pose.pose.position.y;
    ws_msg["data"]["position"]["z"] = msg->pose.pose.position.z;
    ws_msg["data"]["orientation"]["theta"] = theta;
    ws_msg["data"]["velocity"]["linear_x"] = msg->twist.twist.linear.x;
    ws_msg["data"]["velocity"]["angular_z"] = msg->twist.twist.angular.z;

    broadcast_callback_(ws_msg);
}

void ROSBridge::mapCallback(const nav_msgs::msg::OccupancyGrid::SharedPtr msg)
{
    // 다운샘플링: 5초마다
    auto now = this->now();
    if ((now - last_map_time_).seconds() < 5.0) {
        return;
    }
    last_map_time_ = now;

    if (!broadcast_callback_) return;

    // OccupancyGrid → Base64 PNG 변환은 복잡하므로
    // 여기서는 간단히 RLE (Run-Length Encoding) 또는 원본 데이터 전송
    // 실제로는 OpenCV 등으로 PNG 변환 후 Base64 인코딩 필요

    Json::Value ws_msg = createTimestampedMessage("map");
    ws_msg["data"]["info"]["width"] = msg->info.width;
    ws_msg["data"]["info"]["height"] = msg->info.height;
    ws_msg["data"]["info"]["resolution"] = msg->info.resolution;
    ws_msg["data"]["info"]["origin"]["x"] = msg->info.origin.position.x;
    ws_msg["data"]["info"]["origin"]["y"] = msg->info.origin.position.y;

    // 간단한 RLE 인코딩 (예시)
    Json::Value data_array(Json::arrayValue);
    for (size_t i = 0; i < msg->data.size(); i++) {
        data_array.append(msg->data[i]);
    }
    ws_msg["data"]["data"] = data_array;

    broadcast_callback_(ws_msg);
    
    RCLCPP_INFO(this->get_logger(), "Map broadcasted: %dx%d", 
                msg->info.width, msg->info.height);
}

void ROSBridge::pathCallback(const nav_msgs::msg::Path::SharedPtr msg)
{
    if (!broadcast_callback_) return;

    Json::Value ws_msg = createTimestampedMessage("path");
    Json::Value poses_array(Json::arrayValue);

    for (const auto& pose : msg->poses) {
        Json::Value pose_obj;
        pose_obj["x"] = pose.pose.position.x;
        pose_obj["y"] = pose.pose.position.y;
        pose_obj["theta"] = quaternionToYaw(pose.pose.orientation);
        poses_array.append(pose_obj);
    }

    ws_msg["data"]["poses"] = poses_array;
    broadcast_callback_(ws_msg);
}

void ROSBridge::statusCallback(const std_msgs::msg::String::SharedPtr msg)
{
    if (!broadcast_callback_) return;

    // JSON 파싱 시도
    Json::CharReaderBuilder reader;
    Json::Value status_data;
    std::string errors;
    std::istringstream stream(msg->data);

    if (Json::parseFromStream(reader, stream, &status_data, &errors)) {
        Json::Value ws_msg = createTimestampedMessage("status");
        ws_msg["data"] = status_data;
        broadcast_callback_(ws_msg);
    }
}

double ROSBridge::quaternionToYaw(const geometry_msgs::msg::Quaternion& quat)
{
    tf2::Quaternion q(quat.x, quat.y, quat.z, quat.w);
    tf2::Matrix3x3 m(q);
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw);
    return yaw;
}

Json::Value ROSBridge::createTimestampedMessage(const std::string& type)
{
    Json::Value msg;
    msg["type"] = type;
    msg["timestamp"] = this->now().seconds();
    return msg;
}
