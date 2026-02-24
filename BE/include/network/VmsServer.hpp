#ifndef VMS_SERVER_HPP
#define VMS_SERVER_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include "media/RtspProxy.hpp" // For ChannelStats

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Callback types
using OnCoordinateReceived = std::function<void(double x, double y)>;
using OnCommandReceived = std::function<void(int sessionId, const std::string& cmd, int channel)>;

class VmsSession; // Forward declaration

class VmsServer {
public:
    VmsServer(net::io_context& ioc, unsigned short port);
    
    // Start accepting connections
    void start();

    // Send file to a specific VMS client by session ID
    void sendFileToClient(int sessionId, const std::string& filepath);

    // Broadcast stream stats
    void broadcastStats(int channelId, const ChannelStats& stats);

    // Setters for callbacks
    void setCoordinateCallback(OnCoordinateReceived cb) { coordCb = cb; }
    void setCommandCallback(OnCommandReceived cb) { cmdCb = cb; }

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    
    // Keep track of active sessions to send files
    std::vector<std::shared_ptr<VmsSession>> sessions_;
    std::mutex session_mutex_;

    OnCoordinateReceived coordCb;
    OnCommandReceived cmdCb;
};

#endif // VMS_SERVER_HPP
