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
    OnCoordinateReceived& coordCb_;
    OnCommandReceived& cmdCb_;
    std::function<void(std::shared_ptr<VmsSession>)> removeSession_;

public:
    VmsSession(tcp::socket&& socket, OnCoordinateReceived& cCb, OnCommandReceived& cmCb,
               std::function<void(std::shared_ptr<VmsSession>)> rmCb)
        : ws_(std::move(socket)), coordCb_(cCb), cmdCb_(cmCb), removeSession_(rmCb) {}

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
            std::cerr << "❌ [VmsSession] Accept error: " << ec.message() << std::endl;
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

        // 에러 처리 및 세션 종료 로직 (기존과 동일)
        if (ec == websocket::error::closed) { removeSession_(shared_from_this()); return; }
        if (ec) { std::cerr << "❌ Read error: " << ec.message() << std::endl; removeSession_(shared_from_this()); return; }

        try {
            // 1. 메시지 파싱
            std::string msg = beast::buffers_to_string(buffer_.data());
            std::cout << "📥 [VmsSession] Received raw message: " << msg << std::endl;

            auto jsonMsg = json::parse(msg);

            // 2. 필수 헤더 확인
            if (!jsonMsg.contains("type") || !jsonMsg.contains("payload")) {
                std::cerr << "⚠️ Invalid Message Format: Missing 'type' or 'payload'" << std::endl;
                buffer_.consume(buffer_.size()); do_read(); return;
            }

            std::string type = jsonMsg["type"];
            json payload = jsonMsg["payload"];

            // 3. 기능 분기 (Router)
            // [A] 캘리브레이션 좌표 수신
            if (type == "CALIBRATION_CLICK") {
                if (payload.contains("x") && payload.contains("y")) {
                    double x = payload["x"];
                    double y = payload["y"];
                    if (coordCb_) coordCb_(x, y); // 콜백 호출
                    std::cout << "🖱️ Click received: (" << x << ", " << y << ")" << std::endl;
                }
            }
            // [B] 녹화 제어 명령
            else if (type == "RECORD_CONTROL") {
                if (payload.contains("action") && payload.contains("channel_id")) {
                    std::string action = payload["action"];
                    int channel = payload["channel_id"];
                    if (cmdCb_) cmdCb_(action, channel); // 콜백 호출
                    std::cout << "🎥 Record Cmd: " << action << " (Ch " << channel << ")" << std::endl;
                }
            }
            // [C] 추후 확장 가능 (예: PTZ)
            else if (type == "PTZ_CMD") {
                // ... PTZ 처리 로직 ...
            }
            else {
                std::cerr << "⚠️ Unknown Message Type: " << type << std::endl;
            }

        } catch (const json::exception& e) {
            std::cerr << "❌ JSON Parsing Error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ General Error: " << e.what() << std::endl;
        }

        // 버퍼 비우기 및 다음 읽기 대기
        buffer_.consume(buffer_.size());
        do_read();
    }
    

    // Public method to send file
    void sendFile(const std::string& filepath) {
        auto self = shared_from_this();
        net::post(ws_.get_executor(), [self, filepath]() {
            if (filepath.empty()) {
                // Send error notification to client
                json err;
                err["type"] = "file_error";
                err["reason"] = "recording_failed_or_too_short";
                
                self->ws_.text(true);
                self->ws_.write(net::buffer(err.dump()));
                std::cout << "⚠️ [VmsSession] Notify client: Recording too short." << std::endl;
                return;
            }
            self->do_send_file(filepath);
        });
    }

private:
    void do_send_file(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "❌ [VmsSession] Failed to open file: " << filepath << std::endl;
            return;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);

        // 1. Send Metadata (Start)
        json startMsg;
        startMsg["type"] = "FILE_TRANSFER_START";
        startMsg["payload"] = {
            {"filename", filename},
            {"file_size", size}
        };
        
        ws_.text(true);
        ws_.write(net::buffer(startMsg.dump()));

        // 2. Send File Chunks
        ws_.binary(true);
        char buffer[65536]; // 64KB chunk
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            ws_.write(net::buffer(buffer, file.gcount()));
        }

        // 3. Send End Signal (Complete)
        json endMsg;
        endMsg["type"] = "FILE_TRANSFER_COMPLETE";
        endMsg["payload"] = {
            {"filename", filename},
            {"status", "success"}
        };
        
        ws_.text(true);
        ws_.write(net::buffer(endMsg.dump()));

        std::cout << "📤 [VmsSession] Sent file: " << filename << " (" << size << " bytes)" << std::endl;

        // 4. Delete File (Cleanup)
        file.close();
        if (std::remove(filepath.c_str()) == 0) {
            std::cout << "🗑️ [VmsSession] Deleted local file: " << filepath << std::endl;
        } else {
            std::cerr << "⚠️ [VmsSession] Failed to delete file: " << filepath << std::endl;
        }
    }
};

// ============================================================================ 
// VmsServer Implementation
// ============================================================================ 

VmsServer::VmsServer(net::io_context& ioc, unsigned short port)
    : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
}

void VmsServer::start() {
    do_accept();
    std::cout << "✅ [VmsServer] Listening on port " << acceptor_.local_endpoint().port() << std::endl;
}

void VmsServer::broadcastFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    for (auto& session : sessions_) {
        session->sendFile(filepath);
    }
}

void VmsServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&VmsServer::on_accept, this));
}

void VmsServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        std::cerr << "❌ [VmsServer] Accept failed: " << ec.message() << std::endl;
    } else {
        std::string remote_ip = socket.remote_endpoint().address().to_string();
        std::lock_guard<std::mutex> lock(session_mutex_);
        
        auto session = std::make_shared<VmsSession>(
            std::move(socket), coordCb, cmdCb, 
            [this](std::shared_ptr<VmsSession> s) {
                std::lock_guard<std::mutex> lock(session_mutex_);
                sessions_.erase(std::remove(sessions_.begin(), sessions_.end(), s), sessions_.end());
                std::cout << "👋 [VmsServer] Session closed. Total sessions: " << sessions_.size() << std::endl;
            }
        );
        sessions_.push_back(session);
        std::cout << "🤝 [VmsServer] New session from " << remote_ip << ". Total sessions: " << sessions_.size() << std::endl;
        session->run();
    }

    do_accept();
}