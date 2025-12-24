#pragma once

#include <asio.hpp>
#include <memory>
#include <array>
#include "common/connection.h"
#include "common/trackable.h"
#include "adapter/proxy_adapter.h"
#include "constant/metadata.h"
#include "tunnel/tunnel.h"
#include <chrono>
#include <atomic>

namespace clash {
namespace listener {

class Session : public std::enable_shared_from_this<Session>, public common::Trackable {
public:
    Session(asio::ip::tcp::socket socket, std::shared_ptr<tunnel::Tunnel> tunnel);
    ~Session();
    void start();

    // Trackable implementation
    std::string id() const override { return id_; }
    constant::Metadata metadata() const override { return metadata_; }
    uint64_t upload() const override { return upload_; }
    uint64_t download() const override { return download_; }
    std::chrono::system_clock::time_point startTime() const override { return start_time_; }
    std::string chain() const override;
    void close() override;

private:
    void do_read();
    void handle_socks5_handshake(std::size_t length);
    void handle_socks5_request(std::size_t length);
    void handle_http_request(std::size_t length);
    
    void handle_connect();
    void handle_connect_http(bool is_connect, std::size_t initial_data_len);
    void start_relay();
    void do_relay_client_to_target();
    void do_relay_target_to_client();

    asio::ip::tcp::socket socket_;
    std::shared_ptr<tunnel::Tunnel> tunnel_;
    std::unique_ptr<common::Connection> outbound_conn_;
    std::shared_ptr<adapter::ProxyAdapter> adapter_;
    constant::Metadata metadata_;
    
    std::array<char, 8192> buffer_;
    std::array<char, 8192> client_buffer_;
    std::array<char, 8192> target_buffer_;
    
    enum class State {
        Handshake,
        Request,
        Streaming
    };
    State state_ = State::Handshake;

    // Tracking info
    std::string id_;
    std::chrono::system_clock::time_point start_time_;
    std::atomic<uint64_t> upload_{0};
    std::atomic<uint64_t> download_{0};
};

} // namespace listener
} // namespace clash
