#include "listener/session.h"
#include "log/log.h"
#include "common/traffic_manager.h"
#include "common/connection_manager.h"
#include <iostream>
#include <string_view>
#include <random>
#include <sstream>
#include <iomanip>

namespace clash {
namespace listener {

static std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;
    for (int i = 0; i < 8; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << std::hex << dis(gen);
    return ss.str();
}

// 构造函数
// 初始化 Session 对象，分配唯一 ID，记录开始时间
// 获取客户端的源 IP 和端口，用于后续的规则匹配
Session::Session(asio::ip::tcp::socket socket, std::shared_ptr<tunnel::Tunnel> tunnel)
    : socket_(std::move(socket)), tunnel_(std::move(tunnel)) {
    
    // 生成 UUID 用于标识连接
    id_ = generateUUID();
    start_time_ = std::chrono::system_clock::now();

    // 获取远程端点信息 (Source IP/Port)
    std::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (!ec) {
        metadata_.srcIP = endpoint.address().to_string();
        metadata_.srcPort = endpoint.port();
    }
}

Session::~Session() {
    common::ConnectionManager::instance().remove(id_);
}

// 启动 Session：开始读取数据
void Session::start() {
    common::ConnectionManager::instance().add(shared_from_this());
    do_read();
}

std::string Session::chain() const {
    if (adapter_) return adapter_->name();
    return "";
}

void Session::close() {
    std::error_code ec;
    socket_.close(ec);
    if (outbound_conn_) outbound_conn_->close();
}

// 核心读取循环
// 根据当前 Session 的状态 (Handshake, Request, Streaming) 处理读取到的数据
// 初始状态为 Handshake，通过首字节判断是 SOCKS5 还是 HTTP 协议
void Session::do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(asio::buffer(buffer_),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                if (state_ == State::Handshake) {
                    // 协议探测逻辑
                    // SOCKS5 协议第一个字节固定为版本号 0x05
                    // HTTP 协议通常以 GET, POST, CONNECT 等方法开头
                    if (length > 0 && buffer_[0] == 0x05) {
                        handle_socks5_handshake(length);
                    } else {
                        handle_http_request(length);
                    }
                } else if (state_ == State::Request) {
                    // SOCKS5 握手后的请求阶段
                    handle_socks5_request(length);
                } else {
                    // Streaming 状态通常由 relay 函数处理，不应进入此处
                    // 除非有特殊的状态流转逻辑
                }
            } else {
                // 连接断开或读取错误
                log::debug("Session read error: {}", ec.message());
            }
        });
}

// 处理 HTTP 代理请求
// 支持两种模式：
// 1. CONNECT 方法：用于 HTTPS 隧道，建立连接后回复 200 OK
// 2. 普通 HTTP 方法 (GET, POST 等)：解析 Host 头或绝对 URI，直接转发请求
void Session::handle_http_request(std::size_t length) {
    auto self(shared_from_this());
    std::string_view data(buffer_.data(), length);
    
    std::string host;
    int port = 80;

    // 简单的 HTTP 解析逻辑
    // 注意：生产环境应使用更健壮的 HTTP 解析库 (如 llhttp 或 nodejs/http-parser)
    
    // 1. 处理 CONNECT 方法 (HTTPS)
    if (data.substr(0, 7) == "CONNECT") {
        size_t host_start = 8;
        size_t host_end = data.find(' ', host_start);
        if (host_end != std::string::npos) {
            std::string host_port = std::string(data.substr(host_start, host_end - host_start));
            size_t colon = host_port.find(':');
            if (colon != std::string::npos) {
                host = host_port.substr(0, colon);
                port = std::stoi(host_port.substr(colon + 1));
            } else {
                host = host_port;
                port = 443; // CONNECT 默认端口为 443
            }
            
            metadata_.type = constant::Metadata::Type::Http;
        }
    } else {
        // 2. 处理普通 HTTP 代理请求
        // 格式通常为: GET http://example.com/path HTTP/1.1
        // 或者包含 Host 头
        
        // 尝试解析绝对 URI
        size_t protocol_pos = data.find("://");
        if (protocol_pos != std::string::npos) {
            size_t host_start = protocol_pos + 3;
            size_t path_start = data.find('/', host_start);
            if (path_start == std::string::npos) path_start = data.find(' ', host_start);
            
            if (path_start != std::string::npos) {
                std::string host_port = std::string(data.substr(host_start, path_start - host_start));
                size_t colon = host_port.find(':');
                if (colon != std::string::npos) {
                    host = host_port.substr(0, colon);
                    port = std::stoi(host_port.substr(colon + 1));
                } else {
                    host = host_port;
                }
            }
        }
        // 如果找不到绝对 URI，尝试查找 Host 头
        if (host.empty()) {
            size_t host_pos = data.find("\r\nHost: ");
            if (host_pos != std::string::npos) {
                size_t val_start = host_pos + 8;
                size_t val_end = data.find("\r\n", val_start);
                std::string host_port = std::string(data.substr(val_start, val_end - val_start));
                 size_t colon = host_port.find(':');
                if (colon != std::string::npos) {
                    host = host_port.substr(0, colon);
                    port = std::stoi(host_port.substr(colon + 1));
                } else {
                    host = host_port;
                }
            }
        }
    }

    if (host.empty()) {
        log::warn("Failed to parse HTTP request");
        return;
    }

    log::info("HTTP Request to {}:{}", host, std::to_string(port));

    // 设置 Metadata，供路由匹配使用
    metadata_.type = constant::Metadata::Type::Socks5; // 暂时复用 Socks5 类型
    metadata_.host = host;
    metadata_.dstPort = port;

    // 判断是否为 CONNECT 请求
    // CONNECT 请求需要在连接建立后回复 200 OK
    // 普通请求需要将已读取的 buffer_ 转发给目标
    bool is_connect = (data.substr(0, 7) == "CONNECT");

    handle_connect_http(is_connect, length);
}

// 处理 SOCKS5 握手阶段
// 客户端发送版本号和支持的认证方法列表
// 服务端选择一种认证方法并回复
void Session::handle_socks5_handshake(std::size_t length) {
    auto self(shared_from_this());
    
    if (length < 2) {
        log::warn("Socks5 handshake too short");
        return;
    }

    // 验证版本号，必须为 0x05
    if (buffer_[0] != 0x05) {
        log::warn("Invalid Socks5 version: {}", (int)buffer_[0]);
        return;
    }

    int nmethods = buffer_[1];
    // 实际上应该遍历 methods 列表检查是否包含 0x00 (NO AUTH)
    // 这里简化处理，直接回复无需认证

    // 回复客户端：版本 5，选择方法 0x00 (No Authentication Required)
    char reply[] = {0x05, 0x00};
    asio::async_write(socket_, asio::buffer(reply, 2),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                // 握手成功，进入请求阶段
                state_ = State::Request;
                do_read();
            } else {
                log::error("Socks5 handshake write error: {}", ec.message());
            }
        });
}

// 处理 SOCKS5 请求阶段
// 客户端发送连接请求，包含目标地址类型、地址和端口
void Session::handle_socks5_request(std::size_t length) {
    auto self(shared_from_this());

    if (length < 4) {
        log::warn("Socks5 request too short");
        return;
    }

    // 检查命令字，目前仅支持 CONNECT (0x01)
    if (buffer_[1] != 0x01) { 
        log::warn("Unsupported Socks5 command: {}", (int)buffer_[1]);
        // TODO: 应该回复不支持的命令错误
        return;
    }

    std::string host;
    int port = 0;
    int addr_type = buffer_[3];
    
    // 解析目标地址类型
    // 0x01: IPv4
    // 0x03: 域名
    // 0x04: IPv6
    if (addr_type == 0x01) { // IPv4
        if (length < 10) {
            log::warn("Socks5 request too short for IPv4");
            return;
        }
        asio::ip::address_v4::bytes_type bytes;
        std::copy_n(&buffer_[4], 4, bytes.begin());
        host = asio::ip::address_v4(bytes).to_string();
        
        unsigned char p1 = buffer_[8];
        unsigned char p2 = buffer_[9];
        port = (p1 << 8) | p2;
    } else if (addr_type == 0x03) { // Domain
        int domain_len = buffer_[4];
        if (length < 5 + domain_len + 2) {
            log::warn("Socks5 request too short for Domain");
            return;
        }
        host = std::string(&buffer_[5], domain_len);
        // 端口紧跟在域名之后
        unsigned char p1 = buffer_[5 + domain_len];
        unsigned char p2 = buffer_[5 + domain_len + 1];
        port = (p1 << 8) | p2;
    } else if (addr_type == 0x04) { // IPv6
        if (length < 22) {
            log::warn("Socks5 request too short for IPv6");
            return;
        }
        asio::ip::address_v6::bytes_type bytes;
        std::copy_n(&buffer_[4], 16, bytes.begin());
        host = asio::ip::address_v6(bytes).to_string();

        unsigned char p1 = buffer_[20];
        unsigned char p2 = buffer_[21];
        port = (p1 << 8) | p2;
    } else {
        log::warn("Unsupported Socks5 address type: {}", addr_type);
        return;
    }

    log::info("Socks5 Connect Request to {}:{}", host, std::to_string(port));

    // 填充 Metadata
    metadata_.type = constant::Metadata::Type::Socks5;
    metadata_.host = host;
    metadata_.dstPort = port;
    
    // 开始连接目标
    handle_connect();
}

// 建立与目标服务器的连接 (SOCKS5 模式)
// 1. 匹配路由规则，选择合适的代理适配器
// 2. 通过适配器发起连接 (Dial)
// 3. 连接成功后，回复客户端 SOCKS5 成功响应
// 4. 进入 Streaming 状态，开始转发数据
void Session::handle_connect() {
    auto self(shared_from_this());
    
    // 路由匹配
    adapter_ = tunnel_->match(metadata_);
    
    auto& ctx = static_cast<asio::io_context&>(socket_.get_executor().context());

    // 发起连接
    adapter_->dial(metadata_, ctx,
        [this, self](std::error_code ec, std::unique_ptr<common::Connection> conn) {
            if (!ec) {
                outbound_conn_ = std::move(conn);
                log::debug("Connected to target via {}", adapter_->name());
                
                // 连接成功，回复 SOCKS5 成功消息 (0x00)
                // BND.ADDR 和 BND.PORT 这里填 0，表示忽略
                char reply[] = {0x05, 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
                asio::async_write(socket_, asio::buffer(reply, 10),
                    [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                            // 状态流转 -> Streaming
                            state_ = State::Streaming;
                            start_relay();
                        } else {
                            log::error("Socks5 reply write error: {}", ec.message());
                        }
                    });
            } else {
                log::error("Connect error: {}", ec.message());
                // TODO: 应该根据错误类型回复不同的 SOCKS5 错误码
                // 例如 0x04 (Host unreachable), 0x05 (Connection refused) 等
            }
        });
}

// 建立与目标服务器的连接 (HTTP 模式)
// 1. 匹配路由规则
// 2. 发起连接
// 3. 连接成功后：
//    - 如果是 CONNECT 请求，回复 200 Connection Established
//    - 如果是普通请求，将之前读取的请求头数据转发给目标服务器
void Session::handle_connect_http(bool is_connect, std::size_t initial_data_len) {
    auto self(shared_from_this());
    
    adapter_ = tunnel_->match(metadata_);
    auto& ctx = static_cast<asio::io_context&>(socket_.get_executor().context());

    adapter_->dial(metadata_, ctx,
        [this, self, is_connect, initial_data_len](std::error_code ec, std::unique_ptr<common::Connection> conn) {
            if (!ec) {
                outbound_conn_ = std::move(conn);
                log::debug("Connected to target via {}", adapter_->name());
                
                if (is_connect) {
                    // HTTP CONNECT 请求：回复 200 OK，表示隧道建立成功
                    std::string reply = "HTTP/1.1 200 Connection Established\r\n\r\n";
                    asio::async_write(socket_, asio::buffer(reply),
                        [this, self](std::error_code ec, std::size_t /*length*/) {
                            if (!ec) {
                                state_ = State::Streaming;
                                start_relay();
                            } else {
                                log::error("HTTP CONNECT reply write error: {}", ec.message());
                            }
                        });
                } else {
                    // 普通 HTTP 请求：直接转发初始读取的数据 (请求头)
                    outbound_conn_->async_write(asio::buffer(buffer_, initial_data_len),
                        [this, self](std::error_code ec, std::size_t /*length*/) {
                            if (!ec) {
                                state_ = State::Streaming;
                                start_relay();
                            } else {
                                log::error("HTTP initial write error: {}", ec.message());
                            }
                        });
                }
            } else {
                log::error("Connect error: {}", ec.message());
                socket_.close();
            }
        });
}

// 启动双向数据转发
// 同时开启 "客户端 -> 目标" 和 "目标 -> 客户端" 的数据流
void Session::start_relay() {
    do_relay_client_to_target();
    do_relay_target_to_client();
}

// 转发方向：客户端 -> 目标服务器 (Upload)
// 1. 从客户端 Socket 读取数据
// 2. 统计上传流量
// 3. 写入到目标连接 (outbound_conn_)
// 4. 循环执行，直到出错或连接关闭
void Session::do_relay_client_to_target() {
    auto self(shared_from_this());
    socket_.async_read_some(asio::buffer(client_buffer_),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                // 流量统计
                common::TrafficManager::instance().addUpload(length);
                upload_.fetch_add(length, std::memory_order_relaxed);
                
                // 写入目标
                outbound_conn_->async_write(asio::buffer(client_buffer_, length),
                    [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                            // 继续读取下一块数据
                            do_relay_client_to_target();
                        } else {
                            log::debug("Write to target error: {}", ec.message());
                            outbound_conn_->close();
                            socket_.close();
                        }
                    });
            } else {
                log::debug("Read from client error: {}", ec.message());
                outbound_conn_->close();
                socket_.close();
            }
        });
}

// 转发方向：目标服务器 -> 客户端 (Download)
// 1. 从目标连接读取数据
// 2. 统计下载流量
// 3. 写入到客户端 Socket
// 4. 循环执行，直到出错或连接关闭
void Session::do_relay_target_to_client() {
    auto self(shared_from_this());
    outbound_conn_->async_read_some(asio::buffer(target_buffer_),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                // 流量统计
                common::TrafficManager::instance().addDownload(length);
                download_.fetch_add(length, std::memory_order_relaxed);
                
                // 写入客户端
                asio::async_write(socket_, asio::buffer(target_buffer_, length),
                    [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                            // 继续读取下一块数据
                            do_relay_target_to_client();
                        } else {
                            log::debug("Write to client error: {}", ec.message());
                            socket_.close();
                            outbound_conn_->close();
                        }
                    });
            } else {
                log::debug("Read from target error: {}", ec.message());
                socket_.close();
                outbound_conn_->close();
            }
        });
}

} // namespace listener
} // namespace clash
