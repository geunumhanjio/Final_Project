#include "network/VmsServer.hpp"
#include <nlohmann/json.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <fstream>
#include <exception>

using json = nlohmann::json;

// ============================================================================ 
// VmsSession Class
// ============================================================================ 
class VmsSession : public std::enable_shared_from_this<VmsSession> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    int id_; // 세션 ID
    OnCoordinateReceived& coordCb_;
    OnCommandReceived& cmdCb_;
    std::function<void(std::shared_ptr<VmsSession>)> removeSession_;

public:
    VmsSession(tcp::socket&& socket, int id, OnCoordinateReceived& cCb, OnCommandReceived& cmCb,
               std::function<void(std::shared_ptr<VmsSession>)> rmCb)
        : ws_(std::move(socket)), id_(id), coordCb_(cCb), cmdCb_(cmCb), removeSession_(rmCb) {}

    int getId() const { return id_; }
    
    void run() {
        net::dispatch(ws_.get_executor(),
            beast::bind_front_handler(&VmsSession::on_run, shared_from_this()));
    }

    void on_run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.binary(true); // Allow binary messages
        ws_.async_accept(
            beast::bind_front_handler(&VmsSession::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "❌ [VmsSession " << id_ << "] Accept error: " << ec.message() << std::endl;
            return;
        }
        do_read();
    }

    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&VmsSession::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec == websocket::error::closed) {
            std::cout << "👋 [VmsSession " << id_ << "] Connection closed by client." << std::endl;
            removeSession_(shared_from_this());
            return;
        }
        if (ec) {
            std::cerr << "❌ [VmsSession " << id_ << "] Read error: " << ec.message() << std::endl;
            removeSession_(shared_from_this());
            return;
        }

        try {
            std::string msg = beast::buffers_to_string(buffer_.data());
            std::cout << "📥 [VmsSession " << id_ << "] Received raw message: " << msg << std::endl;
            
            auto jsonMsg = json::parse(msg);

            if (!jsonMsg.contains("type") || !jsonMsg.contains("payload")) {
                std::cerr << "⚠️ [VmsSession " << id_ << "] Invalid Message Format" << std::endl;
                buffer_.consume(buffer_.size()); do_read(); return;
            }

            std::string type = jsonMsg["type"];
            json payload = jsonMsg["payload"];

            if (type == "CALIBRATION_CLICK") {
                if (payload.contains("x") && payload.contains("y")) {
                    double x = payload["x"];
                    double y = payload["y"];
                    std::cout << "🖱️ [VmsSession " << id_ << "] Click received: (" << x << ", " << y << ")" << std::endl;
                    if (coordCb_) coordCb_(x, y);
                }
            }
            else if (type == "RECORD_CONTROL") {
                if (payload.contains("action") && payload.contains("channel_id")) {
                    std::string action = payload["action"];
                    int channel = payload["channel_id"];
                    std::cout << "🎥 [VmsSession " << id_ << "] Record Cmd: " << action << " (Ch " << channel << ")" << std::endl;
                    if (cmdCb_) cmdCb_(id_, action, channel); 
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ [VmsSession " << id_ << "] Error parsing message: " << e.what() << std::endl;
        }

        buffer_.consume(buffer_.size());
        do_read();
    }
    
    void sendFile(const std::string& filepath) {
        auto self = shared_from_this();
        net::post(ws_.get_executor(), [self, filepath]() {
            if (filepath.empty()) {
                std::cerr << "⚠️ [VmsSession " << self->id_ << "] Recording failed or too short, notifying client." << std::endl;
                json err;
                err["type"] = "file_error";
                err["reason"] = "recording_failed_or_too_short";
                self->ws_.text(true);
                self->ws_.write(net::buffer(err.dump()));
                return;
            }
            self->do_send_file(filepath);
        });
    }

    void sendStats(int channelId, const ChannelStats& stats) {
        auto self = shared_from_this();
        net::post(ws_.get_executor(), [self, channelId, stats]() {
            json msg;
            msg["type"] = "STREAM_STATS";
            msg["payload"] = {
                {"channel_id", channelId},
                {"fps", stats.fps},
                {"bitrate_kbps", stats.bitrate_kbps},
                {"proxy_latency_ms", stats.proxy_latency_ms}
            };
            try {
                self->ws_.text(true);
                self->ws_.write(net::buffer(msg.dump()));
            } catch (...) {}
        });
    }

private:
    void do_send_file(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "❌ [VmsSession " << id_ << "] Failed to open file: " << filepath << std::endl;
            return;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);

        std::cout << "📤 [VmsSession " << id_ << "] Starting file transfer: " << filename << " (" << size << " bytes)" << std::endl;

        json startMsg;
        startMsg["type"] = "FILE_TRANSFER_START";
        startMsg["payload"] = {{"filename", filename}, {"file_size", size}};
        ws_.text(true);
        ws_.write(net::buffer(startMsg.dump()));

        ws_.binary(true);
        char buffer[65536];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            ws_.write(net::buffer(buffer, file.gcount()));
        }

        json endMsg;
        endMsg["type"] = "FILE_TRANSFER_COMPLETE";
        endMsg["payload"] = {{"filename", filename}, {"status", "success"}};
        ws_.text(true);
        ws_.write(net::buffer(endMsg.dump()));

        std::cout << "✅ [VmsSession " << id_ << "] Successfully sent file: " << filename << std::endl;
        file.close();
        if (std::remove(filepath.c_str()) == 0) {
            std::cout << "🗑️ [VmsSession " << id_ << "] Deleted local file: " << filepath << std::endl;
        }
    }
};

VmsServer::VmsServer(net::io_context& ioc, unsigned short port)
    : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
}

void VmsServer::start() {
    do_accept();
    std::cout << "✅ [VmsServer] Listening on port " << acceptor_.local_endpoint().port() << std::endl;
}

void VmsServer::sendFileToClient(int sessionId, const std::string& filepath) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    for (auto& session : sessions_) {
        if (session->getId() == sessionId) {
            std::cout << "🎯 [VmsServer] Found target session " << sessionId << " for file transfer." << std::endl;
            session->sendFile(filepath);
            return;
        }
    }
    std::cerr << "⚠️ [VmsServer] Target session " << sessionId << " not found. Cleaning up: " << filepath << std::endl;
    std::remove(filepath.c_str());
}

void VmsServer::broadcastStats(int channelId, const ChannelStats& stats) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    for (auto& session : sessions_) {
        session->sendStats(channelId, stats);
    }
}

void VmsServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&VmsServer::on_accept, this));
}

void VmsServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (!ec) {
        static int next_id = 1;
        int current_id = next_id++;
        std::string remote_ip = socket.remote_endpoint().address().to_string();
        
        auto session = std::make_shared<VmsSession>(
            std::move(socket), current_id, coordCb, cmdCb, 
            [this, current_id](std::shared_ptr<VmsSession> s) {
                std::lock_guard<std::mutex> lock(session_mutex_);
                sessions_.erase(std::remove(sessions_.begin(), sessions_.end(), s), sessions_.end());
                std::cout << "👋 [VmsServer] Session " << current_id << " removed. Total: " << sessions_.size() << std::endl;
            }
        );
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sessions_.push_back(session);
        }
        std::cout << "🤝 [VmsServer] New connection " << current_id << " from " << remote_ip << ". Total: " << sessions_.size() << std::endl;
        session->run();
    }
    do_accept();
}
