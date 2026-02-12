#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "rpi_serial_bridge/serial_bridge_node.hpp"

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  
  auto node = std::make_shared<rpi_serial_bridge::SerialBridgeNode>();
  
  rclcpp::spin(node);
  
  rclcpp::shutdown();
  return 0;
}
