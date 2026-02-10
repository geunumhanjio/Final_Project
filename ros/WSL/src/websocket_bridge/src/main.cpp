#include "websocket_bridge/websocket_server.hpp"
#include "websocket_bridge/ros_bridge.hpp"
#include <rclcpp/rclcpp.hpp>
#include <memory>
#include <signal.h>

volatile sig_atomic_t flag = 0;

void signal_handler(int sig)
{
    flag = 1;
}

int main(int argc, char** argv)
{
    // 시그널 핸들러 등록
    signal(SIGINT, signal_handler);

    // ROS2 초기화
    rclcpp::init(argc, argv);

    // ROS Bridge 노드 생성
    auto ros_bridge = std::make_shared<ROSBridge>();

    // WebSocket 서버 생성
    auto ws_server = std::make_shared<WebSocketBridge>(9090);

    // 콜백 연결
    ros_bridge->setBroadcastCallback(
        [ws_server](const Json::Value& msg) {
            ws_server->broadcast(msg);
        });

    ws_server->setMessageCallback(
        [ros_bridge](const Json::Value& msg) {
            ros_bridge->processClientMessage(msg);
        });

    // WebSocket 서버 시작
    ws_server->start();

    // ROS2 스핀
    RCLCPP_INFO(ros_bridge->get_logger(), 
                "WebSocket Bridge running on ws://0.0.0.0:9090");
    
    while (rclcpp::ok() && !flag) {
        rclcpp::spin_some(ros_bridge);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 정리
    ws_server->stop();
    rclcpp::shutdown();

    return 0;
}
