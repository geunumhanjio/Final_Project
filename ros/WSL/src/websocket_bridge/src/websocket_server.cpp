#include "websocket_bridge/websocket_server.hpp"
#include <iostream>

WebSocketBridge::WebSocketBridge(uint16_t port)
    : port_(port), running_(false)
{
    // WebSocket 서버 초기화
    server_.init_asio();
    server_.set_reuse_addr(true);

    // 콜백 설정
    server_.set_open_handler(
        std::bind(&WebSocketBridge::onOpen, this, std::placeholders::_1));
    server_.set_close_handler(
        std::bind(&WebSocketBridge::onClose, this, std::placeholders::_1));
    server_.set_message_handler(
        std::bind(&WebSocketBridge::onMessage, this, 
                  std::placeholders::_1, std::placeholders::_2));

    // 로깅 설정 (필요시)
    server_.clear_access_channels(websocketpp::log::alevel::all);
    server_.set_access_channels(websocketpp::log::alevel::connect);
    server_.set_access_channels(websocketpp::log::alevel::disconnect);
}

WebSocketBridge::~WebSocketBridge()
{
    stop();
}

void WebSocketBridge::start()
{
    if (running_) return;

    running_ = true;
    server_thread_ = std::thread(&WebSocketBridge::runServer, this);
    std::cout << "WebSocket server started on port " << port_ << std::endl;
}

void WebSocketBridge::stop()
{
    if (!running_) return;

    running_ = false;
    server_.stop_listening();
    server_.stop();

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::cout << "WebSocket server stopped" << std::endl;
}

void WebSocketBridge::runServer()
{
    try {
        server_.listen(port_);
        server_.start_accept();
        server_.run();
    } catch (const std::exception& e) {
        std::cerr << "WebSocket server error: " << e.what() << std::endl;
    }
}

void WebSocketBridge::onOpen(ConnectionHandle hdl)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.insert(hdl);
    std::cout << "Client connected. Total clients: " << connections_.size() << std::endl;

    // 연결 확인 메시지 전송
    Json::Value welcome;
    welcome["type"] = "connection";
    welcome["data"]["status"] = "connected";

    Json::StreamWriterBuilder writer;
    std::string message = Json::writeString(writer, welcome);

    try {
        server_.send(hdl, message, websocketpp::frame::opcode::text);
    } catch (const std::exception& e) {
        std::cerr << "Error sending welcome message: " << e.what() << std::endl;
    }
}

void WebSocketBridge::onClose(ConnectionHandle hdl)
{
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(hdl);
    std::cout << "Client disconnected. Total clients: " << connections_.size() << std::endl;
}

void WebSocketBridge::onMessage(ConnectionHandle hdl, WebSocketServer::message_ptr msg)
{
    try {
        std::string payload = msg->get_payload();
        
        // JSON 파싱
        Json::CharReaderBuilder reader;
        Json::Value json_msg;
        std::string errors;
        std::istringstream stream(payload);

        if (!Json::parseFromStream(reader, stream, &json_msg, &errors)) {
            std::cerr << "JSON parse error: " << errors << std::endl;
            return;
        }

        // 콜백 호출
        if (message_callback_) {
            message_callback_(json_msg);
        }

    } catch (const std::exception& e) {
        std::cerr << "Message processing error: " << e.what() << std::endl;
    }
}

void WebSocketBridge::broadcast(const Json::Value& message)
{
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";  // 압축
    std::string json_str = Json::writeString(writer, message);

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& hdl : connections_) {
        try {
            server_.send(hdl, json_str, websocketpp::frame::opcode::text);
        } catch (const std::exception& e) {
            std::cerr << "Error broadcasting message: " << e.what() << std::endl;
        }
    }
}

void WebSocketBridge::setMessageCallback(MessageCallback callback)
{
    message_callback_ = callback;
}
