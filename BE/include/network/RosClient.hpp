#ifndef ROS_CLIENT_HPP
#define ROS_CLIENT_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <string>
#include <memory>
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class RosClient : public std::enable_shared_from_this<RosClient> {
public:
    explicit RosClient(net::io_context& ioc);
    
    // Connect to ROS Bridge (e.g., localhost, 9090)
    void connect(const std::string& host, const std::string& port);
    
    // Publish calibrated coordinates to a topic
    void publishGoal(double x, double y);

private:
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_handshake(beast::error_code ec);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);

    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    std::string host_;
    bool connected_;
};

#endif // ROS_CLIENT_HPP
