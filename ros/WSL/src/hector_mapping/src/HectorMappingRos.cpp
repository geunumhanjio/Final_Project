#include "HectorMappingRos.h"

#include "map/GridMap.h"
#include "util/MapLockerInterface.h"

#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Transform.h>

#include <chrono>
#include <cmath>

// ── helpers ────────────────────────────────────────────────────────────────

static inline double quatToYaw(double qx, double qy, double qz, double qw)
{
  double siny_cosp = 2.0 * (qw * qz + qx * qy);
  double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
  return std::atan2(siny_cosp, cosy_cosp);
}

// Simple mutex wrapper for hectorslam::MapLockerInterface
class MapMutexWrapper : public MapLockerInterface {
public:
  void lockMap()   override { m_.lock(); }
  void unlockMap() override { m_.unlock(); }
private:
  std::mutex m_;
};

// ── constructor ────────────────────────────────────────────────────────────

HectorMappingRos::HectorMappingRos(const rclcpp::NodeOptions& options)
: rclcpp::Node("hector_mapping", options)
{
  // declare & get parameters
  declare_parameter("base_frame",  "base_footprint");
  declare_parameter("map_frame",   "map");
  declare_parameter("odom_frame",  "odom");
  declare_parameter("scan_topic",  "scan");

  declare_parameter("pub_map_odom_transform",     true);
  declare_parameter("pub_map_scanmatch_transform",true);
  declare_parameter("tf_map_scanmatch_transform_frame_name", "scanmatcher_frame");
  declare_parameter("use_tf_scan_transformation", true);
  declare_parameter("pub_odometry",               false);
  declare_parameter("advertise_map_service",       true);
  declare_parameter("scan_subscriber_queue_size",  5);

  declare_parameter("update_factor_free",       0.4);
  declare_parameter("update_factor_occupied",   0.9);
  declare_parameter("map_update_distance_thresh", 0.4);
  declare_parameter("map_update_angle_thresh",   0.06);

  declare_parameter("map_resolution",      0.05);
  declare_parameter("map_size",            2048);
  declare_parameter("map_start_x",         0.5);
  declare_parameter("map_start_y",         0.5);
  declare_parameter("map_multi_res_levels", 2);
  declare_parameter("map_pub_period",       2.0);

  declare_parameter("laser_min_dist",  0.12);
  declare_parameter("laser_max_dist",  8.0);
  declare_parameter("laser_z_min_value", -1.0);
  declare_parameter("laser_z_max_value",  1.0);

  p_base_frame_  = get_parameter("base_frame").as_string();
  p_map_frame_   = get_parameter("map_frame").as_string();
  p_odom_frame_  = get_parameter("odom_frame").as_string();
  p_scan_topic_  = get_parameter("scan_topic").as_string();

  p_pub_map_odom_transform_     = get_parameter("pub_map_odom_transform").as_bool();
  p_pub_map_scanmatch_transform_= get_parameter("pub_map_scanmatch_transform").as_bool();
  p_tf_scanmatch_frame_         = get_parameter("tf_map_scanmatch_transform_frame_name").as_string();
  p_use_tf_scan_transformation_ = get_parameter("use_tf_scan_transformation").as_bool();
  p_pub_odometry_               = get_parameter("pub_odometry").as_bool();
  p_advertise_map_service_      = get_parameter("advertise_map_service").as_bool();
  p_scan_queue_size_            = get_parameter("scan_subscriber_queue_size").as_int();

  p_update_factor_free_         = get_parameter("update_factor_free").as_double();
  p_update_factor_occupied_     = get_parameter("update_factor_occupied").as_double();
  p_map_update_dist_threshold_  = get_parameter("map_update_distance_thresh").as_double();
  p_map_update_angle_threshold_ = get_parameter("map_update_angle_thresh").as_double();

  p_map_resolution_      = get_parameter("map_resolution").as_double();
  p_map_size_            = get_parameter("map_size").as_int();
  p_map_start_x_         = get_parameter("map_start_x").as_double();
  p_map_start_y_         = get_parameter("map_start_y").as_double();
  p_map_multi_res_levels_= get_parameter("map_multi_res_levels").as_int();
  p_map_pub_period_      = get_parameter("map_pub_period").as_double();

  p_laser_min_dist_ = static_cast<float>(get_parameter("laser_min_dist").as_double());
  p_laser_max_dist_ = static_cast<float>(get_parameter("laser_max_dist").as_double());
  p_laser_z_min_    = static_cast<float>(get_parameter("laser_z_min_value").as_double());
  p_laser_z_max_    = static_cast<float>(get_parameter("laser_z_max_value").as_double());

  RCLCPP_INFO(get_logger(), "HectorSM base_frame: %s", p_base_frame_.c_str());
  RCLCPP_INFO(get_logger(), "HectorSM map_frame:  %s", p_map_frame_.c_str());
  RCLCPP_INFO(get_logger(), "HectorSM odom_frame: %s", p_odom_frame_.c_str());
  RCLCPP_INFO(get_logger(), "HectorSM scan_topic: %s", p_scan_topic_.c_str());
  RCLCPP_INFO(get_logger(), "HectorSM pub_map_odom_transform: %s",
    p_pub_map_odom_transform_ ? "true" : "false");
  RCLCPP_INFO(get_logger(), "HectorSM use_tf_scan_transformation: %s",
    p_use_tf_scan_transformation_ ? "true" : "false");

  // ── SLAM processor ──────────────────────────────────────────────────────
  slam_processor_ = new hectorslam::HectorSlamProcessor(
    static_cast<float>(p_map_resolution_),
    p_map_size_, p_map_size_,
    Eigen::Vector2f(p_map_start_x_, p_map_start_y_),
    p_map_multi_res_levels_,
    nullptr, nullptr);
  slam_processor_->setUpdateFactorFree(p_update_factor_free_);
  slam_processor_->setUpdateFactorOccupied(p_update_factor_occupied_);
  slam_processor_->setMapUpdateMinDistDiff(p_map_update_dist_threshold_);
  slam_processor_->setMapUpdateMinAngleDiff(p_map_update_angle_threshold_);

  // only publish top-level map (level 0)
  int pub_levels = 1;
  for (int i = 0; i < pub_levels; ++i) {
    map_pub_containers_.emplace_back();
    slam_processor_->addMapMutex(i, new MapMutexWrapper());

    std::string topic = "map";
    map_pub_containers_[i].mapPublisher_ =
      create_publisher<nav_msgs::msg::OccupancyGrid>(topic, rclcpp::QoS(1).transient_local());
    map_pub_containers_[i].mapMetadataPublisher_ =
      create_publisher<nav_msgs::msg::MapMetaData>(topic + "_metadata", rclcpp::QoS(1).transient_local());

    setMapData(map_pub_containers_[i], slam_processor_->getGridMap(i));
    map_pub_containers_[i].mapMetadataPublisher_->publish(
      map_pub_containers_[i].map_.info);
    // 시작 시 빈 맵 1회 발행 → transient_local 캐시에 저장
    // WebSocket 첫 연결 시 즉시 수신 가능
    map_pub_containers_[i].map_.header.stamp    = this->now();
    map_pub_containers_[i].map_.header.frame_id = p_map_frame_;
    map_pub_containers_[i].mapPublisher_->publish(map_pub_containers_[i].map_);
  }

  // ── TF ─────────────────────────────────────────────────────────────────
  tf_buffer_   = std::make_shared<tf2_ros::Buffer>(get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);
  tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

  // initialise map→odom to identity
  map_to_odom_.rotation.w = 1.0;
  // 초기값: 0으로 두면 첫 스캔에서 바로 발행 가능
  last_map_pub_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);

  // ── Publishers / Subscribers ────────────────────────────────────────────
  pose_pub_        = create_publisher<geometry_msgs::msg::PoseStamped>("slam_out_pose", 1);
  pose_update_pub_ = create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>("poseupdate", 1);

  scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>(
    p_scan_topic_, p_scan_queue_size_,
    std::bind(&HectorMappingRos::scanCallback, this, std::placeholders::_1));

  initial_pose_sub_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
    "initialpose", 2,
    std::bind(&HectorMappingRos::initialPoseCallback, this, std::placeholders::_1));

  // ── Services ────────────────────────────────────────────────────────────
  if (p_advertise_map_service_) {
    get_map_srv_ = create_service<nav_msgs::srv::GetMap>(
      "dynamic_map",
      std::bind(&HectorMappingRos::handleGetMap, this,
                std::placeholders::_1, std::placeholders::_2));
  }
  reset_map_srv_ = create_service<std_srvs::srv::Trigger>(
    "reset_map",
    std::bind(&HectorMappingRos::handleResetMap, this,
              std::placeholders::_1, std::placeholders::_2));
  pause_mapping_srv_ = create_service<std_srvs::srv::SetBool>(
    "pause_mapping",
    std::bind(&HectorMappingRos::handlePauseMapping, this,
              std::placeholders::_1, std::placeholders::_2));

  RCLCPP_INFO(get_logger(), "HectorSM initialised (map %dx%d, res %.3fm), map_pub min_interval=%.1fs",
    p_map_size_, p_map_size_, p_map_resolution_, p_map_pub_period_);
}

HectorMappingRos::~HectorMappingRos()
{
  delete slam_processor_;
}

// ── scan callback ──────────────────────────────────────────────────────────

void HectorMappingRos::scanCallback(const sensor_msgs::msg::LaserScan::SharedPtr scan)
{
  if (pause_scan_processing_) return;

  if (p_use_tf_scan_transformation_) {
    geometry_msgs::msg::TransformStamped laser_tf;
    try {
      laser_tf = tf_buffer_->lookupTransform(
        p_base_frame_, scan->header.frame_id,
        rclcpp::Time(scan->header.stamp),
        rclcpp::Duration::from_seconds(0.5));
    } catch (const tf2::TransformException& e) {
      RCLCPP_WARN(get_logger(),
        "TF lookup %s->%s failed: %s",
        p_base_frame_.c_str(), scan->header.frame_id.c_str(), e.what());
      return;
    }
    rosLaserScanToDataContainerWithTF(*scan, laser_tf, laser_scan_container_,
                                      slam_processor_->getScaleToMap());
  } else {
    rosLaserScanToDataContainer(*scan, laser_scan_container_,
                                slam_processor_->getScaleToMap());
  }

  Eigen::Vector3f start_estimate;
  if (initial_pose_set_) {
    initial_pose_set_ = false;
    start_estimate = initial_pose_;
  } else {
    start_estimate = slam_processor_->getLastScanMatchPose();
  }

  slam_processor_->update(laser_scan_container_, start_estimate);

  // ── 조건부 맵 발행 ────────────────────────────────────────────────────────
  // 맵이 실제로 업데이트됐을 때만, 그리고 p_map_pub_period_ 이상 경과 시 발행
  // (미업데이트 시 발행 안 함 → WebSocket은 transient_local로 마지막 맵 보유)
  {
    const int cur_map_idx = slam_processor_->getGridMap(0).getUpdateIndex();
    if (cur_map_idx != last_map_update_index_) {
      const rclcpp::Time now_t = this->now();
      const double elapsed = (now_t - last_map_pub_time_).seconds();
      if (elapsed >= p_map_pub_period_) {
        std::lock_guard<std::mutex> lk(map_mutex_);
        publishMap(map_pub_containers_[0], slam_processor_->getGridMap(0), now_t);
        last_map_pub_time_ = now_t;
      }
    }
  }

  // ── Pose output ──────────────────────────────────────────────────────────
  const Eigen::Vector3f& pose = slam_processor_->getLastScanMatchPose();
  // TF는 WSL 현재 시각으로 발행 (scan_relay 100ms delay + WiFi jitter로 인해
  // scan->header.stamp이 WSL clock보다 ~200ms 뒤처져 있어 "extrapolation into future" 에러 방지)
  rclcpp::Time stamp = this->now();

  geometry_msgs::msg::PoseStamped ps;
  ps.header.stamp    = stamp;
  ps.header.frame_id = p_map_frame_;
  ps.pose.position.x = pose.x();
  ps.pose.position.y = pose.y();
  ps.pose.orientation.w = cos(pose.z() * 0.5f);
  ps.pose.orientation.z = sin(pose.z() * 0.5f);
  pose_pub_->publish(ps);

  geometry_msgs::msg::PoseWithCovarianceStamped pcs;
  pcs.header = ps.header;
  pcs.pose.pose = ps.pose;
  const Eigen::Matrix3f& cov = slam_processor_->getLastScanMatchCovariance();
  pcs.pose.covariance[0]  = cov(0,0);
  pcs.pose.covariance[7]  = cov(1,1);
  pcs.pose.covariance[35] = cov(2,2);
  pcs.pose.covariance[1] = pcs.pose.covariance[6]  = cov(0,1);
  pcs.pose.covariance[5] = pcs.pose.covariance[30] = cov(0,2);
  pcs.pose.covariance[11]= pcs.pose.covariance[31] = cov(1,2);
  pose_update_pub_->publish(pcs);

  // ── TF output ────────────────────────────────────────────────────────────
  geometry_msgs::msg::TransformStamped tf_out;
  tf_out.header.stamp    = stamp;

  if (p_pub_map_odom_transform_) {
    // Build map→base from SLAM pose
    tf2::Transform map_to_base;
    map_to_base.setOrigin(tf2::Vector3(pose.x(), pose.y(), 0.0));
    tf2::Quaternion q;
    q.setRPY(0, 0, pose.z());
    map_to_base.setRotation(q);

    if (p_odom_frame_ == p_base_frame_) {
      // Scan-only mode: publish map→base_footprint directly
      tf_out.header.frame_id = p_map_frame_;
      tf_out.child_frame_id  = p_base_frame_;
      tf_out.transform       = tf2::toMsg(map_to_base);
    } else {
      // Standard mode: compute map→odom
      geometry_msgs::msg::TransformStamped odom_to_base_tf;
      try {
        odom_to_base_tf = tf_buffer_->lookupTransform(
          p_odom_frame_, p_base_frame_,
          stamp, rclcpp::Duration::from_seconds(0.5));
      } catch (const tf2::TransformException&) {
        odom_to_base_tf.transform.rotation.w = 1.0;
      }

      tf2::Transform odom_to_base;
      tf2::fromMsg(odom_to_base_tf.transform, odom_to_base);

      tf2::Transform map_to_odom = map_to_base * odom_to_base.inverse();
      map_to_odom_ = tf2::toMsg(map_to_odom);

      tf_out.header.frame_id = p_map_frame_;
      tf_out.child_frame_id  = p_odom_frame_;
      tf_out.transform       = map_to_odom_;
    }
    tf_broadcaster_->sendTransform(tf_out);
  }

  if (p_pub_map_scanmatch_transform_) {
    tf2::Transform scanmatch;
    scanmatch.setOrigin(tf2::Vector3(pose.x(), pose.y(), 0.0));
    tf2::Quaternion qs;
    qs.setRPY(0, 0, pose.z());
    scanmatch.setRotation(qs);

    geometry_msgs::msg::TransformStamped sm_tf;
    sm_tf.header.stamp    = stamp;
    sm_tf.header.frame_id = p_map_frame_;
    sm_tf.child_frame_id  = p_tf_scanmatch_frame_;
    sm_tf.transform       = tf2::toMsg(scanmatch);
    tf_broadcaster_->sendTransform(sm_tf);
  }
}

// ── scan conversion ───────────────────────────────────────────────────────

void HectorMappingRos::rosLaserScanToDataContainer(
  const sensor_msgs::msg::LaserScan& scan,
  hectorslam::DataContainer& container, float scaleToMap)
{
  container.clear();
  container.setOrigo(Eigen::Vector2f::Zero());

  float angle = scan.angle_min;
  float maxRange = scan.range_max - 0.1f;

  for (const float& r : scan.ranges) {
    if (r > scan.range_min && r < maxRange
        && r > p_laser_min_dist_ && r < p_laser_max_dist_) {
      float d = r * scaleToMap;
      container.add(Eigen::Vector2f(std::cos(angle) * d, std::sin(angle) * d));
    }
    angle += scan.angle_increment;
  }
}

void HectorMappingRos::rosLaserScanToDataContainerWithTF(
  const sensor_msgs::msg::LaserScan& scan,
  const geometry_msgs::msg::TransformStamped& laser_tf,
  hectorslam::DataContainer& container, float scaleToMap)
{
  container.clear();

  const auto& t = laser_tf.transform.translation;
  const auto& r = laser_tf.transform.rotation;

  // laser origin in base frame
  Eigen::Vector2f laserOrigin(t.x, t.y);
  container.setOrigo(laserOrigin * scaleToMap);

  // yaw of laser frame relative to base frame
  double laser_yaw = quatToYaw(r.x, r.y, r.z, r.w);
  float  cy = static_cast<float>(std::cos(laser_yaw));
  float  sy = static_cast<float>(std::sin(laser_yaw));

  float angle = scan.angle_min;
  float maxRange = scan.range_max - 0.1f;

  for (const float& range : scan.ranges) {
    if (range > scan.range_min && range < maxRange
        && range > p_laser_min_dist_ && range < p_laser_max_dist_) {
      float lx = std::cos(angle) * range;
      float ly = std::sin(angle) * range;
      // rotate to base frame (2D, ignore z)
      float bx = cy * lx - sy * ly + t.x;
      float by = sy * lx + cy * ly + t.y;

      // z check (use laser origin z ~ t.z, point z = 0 after 2D projection)
      float dz = -static_cast<float>(t.z);
      if (dz > p_laser_z_min_ && dz < p_laser_z_max_) {
        container.add(Eigen::Vector2f(bx, by) * scaleToMap);
      }
    }
    angle += scan.angle_increment;
  }
}

// ── map publishing ────────────────────────────────────────────────────────

void HectorMappingRos::publishMap(
  MapPublisherContainer& container,
  const hectorslam::GridMap& gridMap,
  const rclcpp::Time& stamp)
{
  if (last_map_update_index_ != gridMap.getUpdateIndex()) {
    int sizeX = gridMap.getSizeX();
    int sizeY = gridMap.getSizeY();
    int size  = sizeX * sizeY;

    auto& data = container.map_.data;
    std::fill(data.begin(), data.end(), static_cast<int8_t>(-1));

    for (int i = 0; i < size; ++i) {
      if      (gridMap.isFree(i))     data[i] = 0;
      else if (gridMap.isOccupied(i)) data[i] = 100;
    }

    last_map_update_index_ = gridMap.getUpdateIndex();
  }

  container.map_.header.stamp    = stamp;
  container.map_.header.frame_id = p_map_frame_;
  container.mapPublisher_->publish(container.map_);
}

void HectorMappingRos::setMapData(
  MapPublisherContainer& container,
  const hectorslam::GridMap& gridMap)
{
  auto& info = container.map_.info;

  Eigen::Vector2f mapOrigin = gridMap.getWorldCoords(Eigen::Vector2f::Zero());
  mapOrigin.array() -= gridMap.getCellLength() / 2.0f;

  info.origin.position.x    = mapOrigin.x();
  info.origin.position.y    = mapOrigin.y();
  info.origin.orientation.w = 1.0;
  info.resolution           = gridMap.getCellLength();
  info.width                = gridMap.getSizeX();
  info.height               = gridMap.getSizeY();
  container.map_.header.frame_id = p_map_frame_;
  container.map_.data.resize(info.width * info.height, -1);
}

// ── services ──────────────────────────────────────────────────────────────

void HectorMappingRos::handleGetMap(
  const std::shared_ptr<nav_msgs::srv::GetMap::Request>,
  std::shared_ptr<nav_msgs::srv::GetMap::Response> res)
{
  std::lock_guard<std::mutex> lk(map_mutex_);
  res->map = map_pub_containers_[0].map_;
  RCLCPP_INFO(get_logger(), "Map service called");
}

void HectorMappingRos::handleResetMap(
  const std::shared_ptr<std_srvs::srv::Trigger::Request>,
  std::shared_ptr<std_srvs::srv::Trigger::Response> res)
{
  slam_processor_->reset();
  res->success = true;
  RCLCPP_INFO(get_logger(), "Map reset");
}

void HectorMappingRos::handlePauseMapping(
  const std::shared_ptr<std_srvs::srv::SetBool::Request> req,
  std::shared_ptr<std_srvs::srv::SetBool::Response> res)
{
  pause_scan_processing_ = req->data;
  res->success = true;
  RCLCPP_INFO(get_logger(), "Mapping %s",
    pause_scan_processing_ ? "paused" : "resumed");
}

void HectorMappingRos::initialPoseCallback(
  const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg)
{
  const auto& p = msg->pose.pose;
  double yaw = quatToYaw(p.orientation.x, p.orientation.y,
                         p.orientation.z, p.orientation.w);
  initial_pose_ = Eigen::Vector3f(
    static_cast<float>(p.position.x),
    static_cast<float>(p.position.y),
    static_cast<float>(yaw));
  initial_pose_set_ = true;
  RCLCPP_INFO(get_logger(), "Initial pose set: (%.2f, %.2f, %.2f°)",
    initial_pose_.x(), initial_pose_.y(), yaw * 180.0 / M_PI);
}
