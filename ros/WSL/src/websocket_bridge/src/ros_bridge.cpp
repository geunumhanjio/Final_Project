#include "websocket_bridge/ros_bridge.hpp"
#include <chrono>
#include <cmath>

using namespace std::chrono_literals;

ROSBridge::ROSBridge()
    : Node("ros_bridge_node")
{
    // ── 파라미터 선언 및 로드 ─────────────────────────────────────────────
    this->declare_parameter("odom_source", "slam");
    this->declare_parameter("odom_publish_mode", "timer");
    this->declare_parameter("odom_publish_interval", 0.1);

    odom_source_           = this->get_parameter("odom_source").as_string();
    odom_publish_mode_     = this->get_parameter("odom_publish_mode").as_string();
    odom_publish_interval_ = this->get_parameter("odom_publish_interval").as_double();

    RCLCPP_INFO(this->get_logger(),
        "odom_source=%s  odom_publish_mode=%s  interval=%.3fs",
        odom_source_.c_str(), odom_publish_mode_.c_str(), odom_publish_interval_);

    // ── TF2 (slam 소스 전용) ─────────────────────────────────────────────
    if (odom_source_ == "slam") {
        tf_buffer_   = std::make_shared<tf2_ros::Buffer>(this->get_clock());
        tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
    }

    // ── Publishers (Client → ROS2) ────────────────────────────────────────
    nav_cmd_pub_ = this->create_publisher<std_msgs::msg::String>("/nav/command", 10);
    cmd_vel_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    mode_pub_    = this->create_publisher<std_msgs::msg::String>("/mode_control", 10);
    estop_pub_   = this->create_publisher<std_msgs::msg::Bool>("/emergency_stop", 10);
    tracking_enable_pub_       = this->create_publisher<std_msgs::msg::Bool>("/tracking/enable", 10);
    tilt_pub_                  = this->create_publisher<std_msgs::msg::Float32>("/camera/tilt", 10);
    fall_detection_enable_pub_ = this->create_publisher<std_msgs::msg::Bool>("/fall_detection/enable", 10);

    // ── Subscribers (ROS2 → Client) ───────────────────────────────────────
    // /odom: 항상 구독 (속도 캐시 + odom 소스 + topic 모드)
    odom_sub_ = this->create_subscription<nav_msgs::msg::Odometry>(
        "/odom", 10,
        std::bind(&ROSBridge::odomCallback, this, std::placeholders::_1));

    map_sub_ = this->create_subscription<nav_msgs::msg::OccupancyGrid>(
        "/map", rclcpp::QoS(rclcpp::KeepLast(1)).transient_local().reliable(),
        std::bind(&ROSBridge::mapCallback, this, std::placeholders::_1));

    path_sub_ = this->create_subscription<nav_msgs::msg::Path>(
        "/plan", 10,
        std::bind(&ROSBridge::pathCallback, this, std::placeholders::_1));

    nav_status_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/nav/status", 10,
        std::bind(&ROSBridge::navStatusCallback, this, std::placeholders::_1));

    nav_feedback_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/nav/feedback", 10,
        std::bind(&ROSBridge::navFeedbackCallback, this, std::placeholders::_1));

    tracking_status_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/tracking/status", 10,
        std::bind(&ROSBridge::trackingStatusCallback, this, std::placeholders::_1));

    fall_alert_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/fall_detection/alert", 10,
        std::bind(&ROSBridge::fallAlertCallback, this, std::placeholders::_1));

    fall_status_sub_ = this->create_subscription<std_msgs::msg::String>(
        "/fall_detection/status", 10,
        std::bind(&ROSBridge::fallStatusCallback, this, std::placeholders::_1));

    // ── odom 타이머 (timer 모드) ──────────────────────────────────────────
    if (odom_publish_mode_ == "timer") {
        auto interval_ms = std::chrono::milliseconds(
            static_cast<int64_t>(odom_publish_interval_ * 1000.0));
        odom_timer_ = this->create_wall_timer(
            interval_ms,
            std::bind(&ROSBridge::odomTimerCallback, this));
    }

    last_odom_time_ = this->now();

    RCLCPP_INFO(this->get_logger(), "ROS Bridge initialized");
}

void ROSBridge::setBroadcastCallback(BroadcastCallback callback)
{
    broadcast_callback_ = callback;
}

// ── odom 콜백: 캐시 + topic 모드일 때 전송 ───────────────────────────────

void ROSBridge::odomCallback(const nav_msgs::msg::Odometry::SharedPtr msg)
{
    // 항상 최신 odom 캐시 (속도 정보)
    {
        std::lock_guard<std::mutex> lock(odom_mutex_);
        latest_odom_ = msg;
    }

    // topic 모드에서만 바로 전송
    if (odom_publish_mode_ != "topic") return;

    auto now = this->now();
    if ((now - last_odom_time_).seconds() < odom_publish_interval_) return;
    last_odom_time_ = now;

    if (!broadcast_callback_) return;

    if (odom_source_ == "slam") {
        // topic 모드 + slam 소스: TF 조회 후 전송
        try {
            auto tf = tf_buffer_->lookupTransform(
                "map", "base_footprint", tf2::TimePointZero);
            double theta = quaternionToYaw(tf.transform.rotation);
            broadcastOdom(
                tf.transform.translation.x,
                tf.transform.translation.y,
                theta,
                msg->twist.twist.linear.x,
                msg->twist.twist.angular.z);
        } catch (const tf2::TransformException & ex) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                "TF map→base_footprint 조회 실패: %s", ex.what());
        }
    } else {
        // topic 모드 + odom 소스
        double theta = quaternionToYaw(msg->pose.pose.orientation);
        broadcastOdom(
            msg->pose.pose.position.x,
            msg->pose.pose.position.y,
            theta,
            msg->twist.twist.linear.x,
            msg->twist.twist.angular.z);
    }
}

// ── odom 타이머 콜백 (timer 모드) ────────────────────────────────────────

void ROSBridge::odomTimerCallback()
{
    if (!broadcast_callback_) return;

    // 속도는 /odom 캐시에서 가져옴
    double vx = 0.0, vz = 0.0;
    {
        std::lock_guard<std::mutex> lock(odom_mutex_);
        if (latest_odom_) {
            vx = latest_odom_->twist.twist.linear.x;
            vz = latest_odom_->twist.twist.angular.z;
        }
    }

    if (odom_source_ == "slam") {
        // TF lookup: map → base_footprint (SLAM 기준 위치)
        try {
            auto tf = tf_buffer_->lookupTransform(
                "map", "base_footprint", tf2::TimePointZero);
            double theta = quaternionToYaw(tf.transform.rotation);
            broadcastOdom(
                tf.transform.translation.x,
                tf.transform.translation.y,
                theta, vx, vz);
        } catch (const tf2::TransformException & ex) {
            RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                "TF map→base_footprint 조회 실패: %s", ex.what());
        }
    } else {
        // /odom 캐시 데이터 사용 (EKF 오도메트리 기준)
        std::lock_guard<std::mutex> lock(odom_mutex_);
        if (!latest_odom_) return;
        double theta = quaternionToYaw(latest_odom_->pose.pose.orientation);
        broadcastOdom(
            latest_odom_->pose.pose.position.x,
            latest_odom_->pose.pose.position.y,
            theta, vx, vz);
    }
}

// ── broadcastOdom 헬퍼 ────────────────────────────────────────────────────

void ROSBridge::broadcastOdom(double x, double y, double theta, double vx, double vz)
{
    Json::Value ws_msg = createTimestampedMessage("odom");
    ws_msg["data"]["position"]["x"] = x;
    ws_msg["data"]["position"]["y"] = y;
    ws_msg["data"]["position"]["z"] = 0.0;
    ws_msg["data"]["orientation"]["theta"] = theta;
    ws_msg["data"]["velocity"]["linear_x"] = vx;
    ws_msg["data"]["velocity"]["angular_z"] = vz;
    ws_msg["data"]["source"] = odom_source_;  // 클라이언트에서 소스 구분 가능
    broadcast_callback_(ws_msg);
}

// ── 나머지 콜백들 ─────────────────────────────────────────────────────────

void ROSBridge::processClientMessage(const Json::Value& message)
{
    std::string type = message["type"].asString();
    Json::Value data = message["data"];

    if (type == "nav_command") {
        handleNavCommand(data);
    } else if (type == "cmd_vel") {
        handleCmdVel(data);
    } else if (type == "mode_control") {
        handleModeControl(data);
    } else if (type == "emergency_stop") {
        handleEmergencyStop(data);
    } else if (type == "tracking_enable") {
        auto msg = std_msgs::msg::Bool();
        msg.data = data.get("enable", false).asBool();
        tracking_enable_pub_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "Tracking %s", msg.data ? "enabled" : "disabled");
    } else if (type == "fall_detection_enable") {
        auto msg = std_msgs::msg::Bool();
        msg.data = data.get("enable", false).asBool();
        fall_detection_enable_pub_->publish(msg);
        RCLCPP_INFO(this->get_logger(), "Fall detection %s", msg.data ? "enabled" : "disabled");
    } else if (type == "camera_tilt") {
        auto msg = std_msgs::msg::Float32();
        msg.data = static_cast<float>(data.get("angle", 0.0).asDouble());
        tilt_pub_->publish(msg);
    } else {
        RCLCPP_WARN(this->get_logger(), "Unknown message type: %s", type.c_str());
    }
}

void ROSBridge::handleNavCommand(const Json::Value& data)
{
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string cmd_str = Json::writeString(writer, data);

    auto msg = std_msgs::msg::String();
    msg.data = cmd_str;
    nav_cmd_pub_->publish(msg);

    RCLCPP_INFO(this->get_logger(), "Nav command published: %s", cmd_str.c_str());
}

void ROSBridge::handleCmdVel(const Json::Value& data)
{
    auto twist_msg = geometry_msgs::msg::Twist();
    twist_msg.linear.x  = data.get("linear_x", 0.0).asDouble();
    twist_msg.linear.y  = data.get("linear_y", 0.0).asDouble();
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
        auto twist_msg = geometry_msgs::msg::Twist();
        cmd_vel_pub_->publish(twist_msg);
        RCLCPP_WARN(this->get_logger(), "EMERGENCY STOP ACTIVATED!");
    }
}

void ROSBridge::mapCallback(const nav_msgs::msg::OccupancyGrid::SharedPtr msg)
{
    // HectorSLAM이 이미 업데이트 시에만 발행하므로 추가 쓰로틀 불필요
    if (!broadcast_callback_) return;

    Json::Value ws_msg = createTimestampedMessage("map");
    ws_msg["data"]["info"]["width"]      = msg->info.width;
    ws_msg["data"]["info"]["height"]     = msg->info.height;
    ws_msg["data"]["info"]["resolution"] = msg->info.resolution;
    ws_msg["data"]["info"]["origin"]["x"] = msg->info.origin.position.x;
    ws_msg["data"]["info"]["origin"]["y"] = msg->info.origin.position.y;

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
        pose_obj["x"]     = pose.pose.position.x;
        pose_obj["y"]     = pose.pose.position.y;
        pose_obj["theta"] = quaternionToYaw(pose.pose.orientation);
        poses_array.append(pose_obj);
    }

    ws_msg["data"]["poses"] = poses_array;
    broadcast_callback_(ws_msg);
}

void ROSBridge::navStatusCallback(const std_msgs::msg::String::SharedPtr msg)
{
    if (!broadcast_callback_) return;

    Json::CharReaderBuilder reader;
    Json::Value status_data;
    std::string errors;
    std::istringstream stream(msg->data);

    if (Json::parseFromStream(reader, stream, &status_data, &errors)) {
        Json::Value ws_msg = createTimestampedMessage("nav_status");
        ws_msg["data"] = status_data;
        broadcast_callback_(ws_msg);
    } else {
        RCLCPP_WARN(this->get_logger(), "nav_status JSON parse error: %s", errors.c_str());
    }
}

void ROSBridge::navFeedbackCallback(const std_msgs::msg::String::SharedPtr msg)
{
    if (!broadcast_callback_) return;

    Json::CharReaderBuilder reader;
    Json::Value feedback_data;
    std::string errors;
    std::istringstream stream(msg->data);

    if (Json::parseFromStream(reader, stream, &feedback_data, &errors)) {
        Json::Value ws_msg = createTimestampedMessage("nav_feedback");
        ws_msg["data"] = feedback_data;
        broadcast_callback_(ws_msg);
    } else {
        RCLCPP_WARN(this->get_logger(), "nav_feedback JSON parse error: %s", errors.c_str());
    }
}

void ROSBridge::trackingStatusCallback(const std_msgs::msg::String::SharedPtr msg)
{
    if (!broadcast_callback_) return;

    Json::CharReaderBuilder reader;
    Json::Value tracking_data;
    std::string errors;
    std::istringstream stream(msg->data);

    if (Json::parseFromStream(reader, stream, &tracking_data, &errors)) {
        Json::Value ws_msg = createTimestampedMessage("tracking_status");
        ws_msg["data"] = tracking_data;
        broadcast_callback_(ws_msg);
    } else {
        RCLCPP_WARN(this->get_logger(), "tracking_status JSON parse error: %s", errors.c_str());
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
    msg["type"]      = type;
    msg["timestamp"] = this->now().seconds();
    return msg;
}

void ROSBridge::fallAlertCallback(const std_msgs::msg::String::SharedPtr msg)
{
    if (!broadcast_callback_) return;

    Json::CharReaderBuilder reader;
    Json::Value alert_data;
    std::string errors;
    std::istringstream stream(msg->data);

    if (Json::parseFromStream(reader, stream, &alert_data, &errors)) {
        Json::Value ws_msg = createTimestampedMessage("fall_alert");
        ws_msg["data"] = alert_data;
        broadcast_callback_(ws_msg);
        RCLCPP_WARN(this->get_logger(), "Fall alert broadcasted to clients");
    } else {
        RCLCPP_WARN(this->get_logger(), "fall_alert JSON parse error: %s", errors.c_str());
    }
}

void ROSBridge::fallStatusCallback(const std_msgs::msg::String::SharedPtr msg)
{
    if (!broadcast_callback_) return;

    Json::CharReaderBuilder reader;
    Json::Value status_data;
    std::string errors;
    std::istringstream stream(msg->data);

    if (Json::parseFromStream(reader, stream, &status_data, &errors)) {
        Json::Value ws_msg = createTimestampedMessage("fall_detection_status");
        ws_msg["data"] = status_data;
        broadcast_callback_(ws_msg);
    } else {
        RCLCPP_WARN(this->get_logger(), "fall_status JSON parse error: %s", errors.c_str());
    }
}
