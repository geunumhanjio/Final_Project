#pragma once

#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/buffer.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <sensor_msgs/msg/laser_scan.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/map_meta_data.hpp>
#include <nav_msgs/srv/get_map.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <std_srvs/srv/set_bool.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>

#include "slam_main/HectorSlamProcessor.h"
#include "scan/DataPointContainer.h"

#include <mutex>

struct MapPublisherContainer {
  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr mapPublisher_;
  rclcpp::Publisher<nav_msgs::msg::MapMetaData>::SharedPtr mapMetadataPublisher_;
  nav_msgs::msg::OccupancyGrid map_;
};

class HectorMappingRos : public rclcpp::Node
{
public:
  explicit HectorMappingRos(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  ~HectorMappingRos();

private:
  void scanCallback(const sensor_msgs::msg::LaserScan::SharedPtr scan);
  void handleGetMap(
    const std::shared_ptr<nav_msgs::srv::GetMap::Request> req,
    std::shared_ptr<nav_msgs::srv::GetMap::Response> res);
  void handleResetMap(
    const std::shared_ptr<std_srvs::srv::Trigger::Request> req,
    std::shared_ptr<std_srvs::srv::Trigger::Response> res);
  void handlePauseMapping(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> req,
    std::shared_ptr<std_srvs::srv::SetBool::Response> res);
  void initialPoseCallback(
    const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg);

  void publishMap(MapPublisherContainer& container,
                  const hectorslam::GridMap& gridMap,
                  const rclcpp::Time& stamp);
  void setMapData(MapPublisherContainer& container,
                  const hectorslam::GridMap& gridMap);

  void rosLaserScanToDataContainer(
    const sensor_msgs::msg::LaserScan& scan,
    hectorslam::DataContainer& container, float scaleToMap);

  void rosLaserScanToDataContainerWithTF(
    const sensor_msgs::msg::LaserScan& scan,
    const geometry_msgs::msg::TransformStamped& laser_tf,
    hectorslam::DataContainer& container, float scaleToMap);

  // ── ROS handles ────────────────────────────────────────────
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr initial_pose_sub_;

  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pose_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr pose_update_pub_;

  rclcpp::Service<nav_msgs::srv::GetMap>::SharedPtr get_map_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_map_srv_;
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr pause_mapping_srv_;

  std::vector<MapPublisherContainer> map_pub_containers_;
  rclcpp::Time last_map_pub_time_;

  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  // ── SLAM core ────────────────────────────────────────────
  hectorslam::HectorSlamProcessor* slam_processor_;
  hectorslam::DataContainer laser_scan_container_;

  int last_map_update_index_{-100};
  std::mutex map_mutex_;
  bool pause_scan_processing_{false};
  bool initial_pose_set_{false};
  Eigen::Vector3f initial_pose_{Eigen::Vector3f::Zero()};

  // map→odom transform (kept between scans)
  geometry_msgs::msg::Transform map_to_odom_;

  // ── Parameters ──────────────────────────────────────────
  std::string p_base_frame_;
  std::string p_map_frame_;
  std::string p_odom_frame_;
  std::string p_scan_topic_;

  bool p_pub_map_odom_transform_;
  bool p_pub_map_scanmatch_transform_;
  std::string p_tf_scanmatch_frame_;

  bool p_use_tf_scan_transformation_;
  bool p_pub_odometry_;
  bool p_advertise_map_service_;
  int  p_scan_queue_size_;

  double p_update_factor_free_;
  double p_update_factor_occupied_;
  double p_map_update_dist_threshold_;
  double p_map_update_angle_threshold_;

  double p_map_resolution_;
  int    p_map_size_;
  double p_map_start_x_;
  double p_map_start_y_;
  int    p_map_multi_res_levels_;
  double p_map_pub_period_;

  float p_laser_min_dist_;
  float p_laser_max_dist_;
  float p_laser_z_min_;
  float p_laser_z_max_;
};
