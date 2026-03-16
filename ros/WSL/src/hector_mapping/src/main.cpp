#include <rclcpp/rclcpp.hpp>
#include "HectorMappingRos.h"

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<HectorMappingRos>());
  rclcpp::shutdown();
  return 0;
}
