#ifndef WEBSOCKET_BRIDGE__WEBSOCKET_SERVER_HPP_
#define WEBSOCKET_BRIDGE__WEBSOCKET_SERVER_HPP_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <json/json.h>
#include <functional>
#include <set>
#include <mutex>
#include <thread>

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;
typedef websocketpp::connection_hdl ConnectionHandle;

class WebSocketBridge
{
public:
    using MessageCallback = std::function<void(const Json::Value&)>;

    WebSocketBridge(uint16_t port);
    ~WebSocketBridge();

    void start();
    void stop();
    void broadcast(const Json::Value& message);
    void setMessageCallback(MessageCallback callback);

private:
    void onOpen(ConnectionHandle hdl);
    void onClose(ConnectionHandle hdl);
    void onMessage(ConnectionHandle hdl, WebSocketServer::message_ptr msg);
    void runServer();

    WebSocketServer server_;
    uint16_t port_;
    std::set<ConnectionHandle, std::owner_less<ConnectionHandle>> connections_;
    std::mutex mutex_;
    std::thread server_thread_;
    MessageCallback message_callback_;
    bool running_;
};

#endif  // WEBSOCKET_BRIDGE__WEBSOCKET_SERVER_HPP_
