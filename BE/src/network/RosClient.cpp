#include "network/RosClient.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

RosClient::RosClient(net::io_context& ioc)
    : resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc)), connected_(false) {
}

void RosClient::connect(const std::string& host, const std::string& port) {
    host_ = host;
    resolver_.async_resolve(host, port,
        beast::bind_front_handler(&RosClient::on_resolve, shared_from_this()));
}

void RosClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        std::cerr << "❌ [RosClient] Resolve error: " << ec.message() << std::endl;
        return;
    }

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    beast::get_lowest_layer(ws_).async_connect(results,
        beast::bind_front_handler(&RosClient::on_connect, shared_from_this()));
}

void RosClient::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
    if (ec) {
        std::cerr << "❌ [RosClient] Connect error: " << ec.message() << std::endl;
        return;
    }

    beast::get_lowest_layer(ws_).expires_never();
    
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    ws_.async_handshake(host_, "/",
        beast::bind_front_handler(&RosClient::on_handshake, shared_from_this()));
}

void RosClient::on_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "❌ [RosClient] Handshake error: " << ec.message() << std::endl;
        return;
    }
    
    connected_ = true;
    std::cout << "✅ [RosClient] Connected to ROS Bridge!" << std::endl;
    
    // Optional: Advertise topic here if needed
}

void RosClient::publishGoal(double x, double y) {
    if (!connected_) {
        std::cerr << "⚠️ [RosClient] Not connected, cannot publish." << std::endl;
        return;
    }

    // Construct ROS Bridge JSON message
    // Topic: /calibrated_goal
    // Type: geometry_msgs/Point
    json msg;
    msg["x"] = x;
    msg["y"] = y;
    msg["z"] = 0.0;

    json payload;
    payload["op"] = "publish";
    payload["topic"] = "/calibrated_goal";
    payload["type"] = "geometry_msgs/Point";
    payload["msg"] = msg;

    auto str = std::make_shared<std::string>(payload.dump());

    ws_.async_write(
        net::buffer(*str),
        beast::bind_front_handler(&RosClient::on_write, shared_from_this()));
}

void RosClient::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    if (ec) {
        std::cerr << "❌ [RosClient] Write error: " << ec.message() << std::endl;
    }
}
