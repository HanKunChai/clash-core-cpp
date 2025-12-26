#include "control/http_controller.h"
#include "log/log.h"
#include "adapter/selector.h"
#include "adapter/url_test.h"
#include "adapter/fallback.h"
#include "common/traffic_manager.h"
#include "common/connection_manager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

namespace clash
{
namespace control
{

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
public:
    HttpSession(asio::ip::tcp::socket socket, std::shared_ptr<tunnel::Tunnel> tunnel)
        : socket_(std::move(socket)), tunnel_(std::move(tunnel)), timer_(socket.get_executor()) {}

    ~HttpSession()
    {
        if (log_sub_id_ != -1)
        {
            log::unsubscribe(log_sub_id_);
        }
    }

    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(buffer_),
            [this, self](std::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                    handle_request(length);
                }
            });
    }

    void handle_request(std::size_t length)
    {
        std::string request(buffer_.data(), length);
        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;

        // Simple routing
        if (method == "GET" && path == "/proxies")
        {
            handle_get_proxies();
        }
        else if (method == "GET" && path == "/traffic")
        {
            handle_traffic();
        }
        else if (method == "GET" && path == "/connections")
        {
            handle_get_connections();
        }
        else if (method == "GET" && path.find("/logs") == 0)
        {
            // Parse query string for level
            std::string level = "info";
            size_t q = path.find("?level=");
            if (q != std::string::npos)
            {
                level = path.substr(q + 7);
            }
            handle_logs(level);
        }
        else if (method == "DELETE" && path.find("/connections/") == 0)
        {
            std::string id = path.substr(13); // /connections/
            handle_close_connection(id);
        }
        else if (method == "PUT" && path.find("/proxies/") == 0)
        {
            std::string name = path.substr(9); // /proxies/
            // Decode URL encoding if needed (skipping for now)
            
            // Read body
            size_t body_pos = request.find("\r\n\r\n");
            if (body_pos != std::string::npos)
            {
                std::string body = request.substr(body_pos + 4);
                handle_put_proxy(name, body);
            }
            else
            {
                send_response(400, "Bad Request");
            }
        }
        else
        {
            send_response(404, "Not Found");
        }
    }

    // 处理流量请求：建立长连接并定期推送流量数据
    void handle_traffic()
    {
        auto self(shared_from_this());
        std::string response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Transfer-Encoding: chunked\r\n"
                               "Connection: keep-alive\r\n"
                               "\r\n";
        asio::async_write(socket_, asio::buffer(response),
            [this, self](std::error_code ec, std::size_t)
            {
                if (!ec)
                {
                    send_traffic_update();
                }
            });
    }

    // 发送流量更新数据
    void send_traffic_update()
    {
        auto self(shared_from_this());
        
        json j;
        j["up"] = common::TrafficManager::instance().totalUpload();
        j["down"] = common::TrafficManager::instance().totalDownload();
        
        std::string data = j.dump() + "\n";
        std::stringstream ss;
        ss << std::hex << data.length() << "\r\n" << data << "\r\n";
        std::string chunk = ss.str();

        asio::async_write(socket_, asio::buffer(chunk),
            [this, self](std::error_code ec, std::size_t)
            {
                if (!ec)
                {
                    timer_.expires_after(std::chrono::seconds(1));
                    timer_.async_wait([this, self](std::error_code ec)
                    {
                        if (!ec)
                        {
                            send_traffic_update();
                        }
                    });
                }
            });
    }

    // 获取当前所有连接信息
    void handle_get_connections()
    {
        json root;
        root["downloadTotal"] = common::TrafficManager::instance().totalDownload();
        root["uploadTotal"] = common::TrafficManager::instance().totalUpload();
        
        json connections = json::array();
        auto conns = common::ConnectionManager::instance().getAll();
        
        for (const auto& conn : conns)
        {
            json c;
            c["id"] = conn->id();
            
            json metadata;
            auto meta = conn->metadata();
            metadata["network"] = "tcp"; // Assuming TCP for now
            metadata["type"] = "Socks5"; // Simplified
            metadata["sourceIP"] = meta.srcIP;
            metadata["sourcePort"] = std::to_string(meta.srcPort);
            metadata["destinationIP"] = meta.dstIP;
            metadata["destinationPort"] = std::to_string(meta.dstPort);
            metadata["host"] = meta.host;
            
            c["metadata"] = metadata;
            c["upload"] = conn->upload();
            c["download"] = conn->download();
            
            auto start = conn->startTime();
            auto now = std::chrono::system_clock::now();
            auto start_time_t = std::chrono::system_clock::to_time_t(start);
            
            // Format time as ISO 8601
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&start_time_t), "%FT%TZ");
            c["start"] = ss.str();
            
            c["chains"] = json::array({conn->chain()});
            c["rule"] = ""; // TODO: Store matched rule
            c["rulePayload"] = "";
            
            connections.push_back(c);
        }
        
        root["connections"] = connections;
        send_response(200, root.dump());
    }

    // 关闭指定连接
    void handle_close_connection(const std::string& id)
    {
        common::ConnectionManager::instance().close(id);
        send_response(204, "");
    }

    void handle_get_proxies()
    {
        json root;
        json proxies = json::object();

        auto& all_proxies = tunnel_->proxies();
        for (const auto& [name, proxy] : all_proxies)
        {
            json p;
            p["name"] = proxy->name();
            
            switch (proxy->type())
            {
                case constant::AdapterType::Direct: p["type"] = "Direct"; break;
                case constant::AdapterType::Reject: p["type"] = "Reject"; break;
                case constant::AdapterType::Socks5: p["type"] = "Socks5"; break;
                case constant::AdapterType::Shadowsocks: p["type"] = "Shadowsocks"; break;
                case constant::AdapterType::Vmess: p["type"] = "Vmess"; break;
                case constant::AdapterType::Selector:
                {
                    p["type"] = "Selector";
                    auto selector = std::dynamic_pointer_cast<adapter::SelectorAdapter>(proxy);
                    if (selector)
                    {
                        p["now"] = selector->selected();
                        p["all"] = selector->all();
                    }
                    break;
                }
                case constant::AdapterType::URLTest: p["type"] = "URLTest"; break;
                case constant::AdapterType::Fallback: p["type"] = "Fallback"; break;
                case constant::AdapterType::LoadBalance: p["type"] = "LoadBalance"; break;
                default: p["type"] = "Unknown"; break;
            }
            proxies[name] = p;
        }
        root["proxies"] = proxies;
        send_response(200, root.dump());
    }

    void handle_put_proxy(const std::string& group_name, const std::string& body)
    {
        try
        {
            auto j = json::parse(body);
            std::string proxy_name = j["name"];

            auto proxy = tunnel_->proxy(group_name);
            if (!proxy)
            {
                send_response(404, "Proxy group not found");
                return;
            }

            if (proxy->type() == constant::AdapterType::Selector)
            {
                auto selector = std::dynamic_pointer_cast<adapter::SelectorAdapter>(proxy);
                selector->select(proxy_name);
                send_response(204, "");
            }
            else
            {
                send_response(400, "Not a selector group");
            }
        }
        catch (...)
        {
            send_response(400, "Invalid JSON");
        }
    }

    // 处理日志请求：订阅日志系统并实时推送
    void handle_logs(const std::string& level_str)
    {
        auto self(shared_from_this());
        std::string response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Transfer-Encoding: chunked\r\n"
                               "Connection: keep-alive\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "\r\n";
        
        asio::async_write(socket_, asio::buffer(response),
            [this, self, level_str](std::error_code ec, std::size_t)
            {
                if (!ec)
                {
                    log::Level target_level = log::Level::Info;
                    if (level_str == "debug") target_level = log::Level::Debug;
                    else if (level_str == "warning") target_level = log::Level::Warning;
                    else if (level_str == "error") target_level = log::Level::Error;
                    else if (level_str == "silent") target_level = log::Level::Silent;

                    log_sub_id_ = log::subscribe([this, self, target_level](log::Level level, const std::string& msg)
                    {
                        if (level < target_level) return;

                        json j;
                        switch (level)
                        {
                            case log::Level::Debug: j["type"] = "debug"; break;
                            case log::Level::Info: j["type"] = "info"; break;
                            case log::Level::Warning: j["type"] = "warning"; break;
                            case log::Level::Error: j["type"] = "error"; break;
                            default: j["type"] = "unknown"; break;
                        }
                        j["payload"] = msg;

                        std::string data = j.dump() + "\n";
                        std::stringstream ss;
                        ss << std::hex << data.length() << "\r\n" << data << "\r\n";
                        auto chunk = std::make_shared<std::string>(ss.str());

                        asio::post(socket_.get_executor(), [this, self, chunk]()
                        {
                            asio::async_write(socket_, asio::buffer(*chunk),
                                [this, self, chunk](std::error_code ec, std::size_t)
                                {
                                    if (ec)
                                    {
                                        socket_.close();
                                    }
                                });
                        });
                    });
                }
            });
    }

    void send_response(int status, const std::string& body)
    {
        std::string status_msg = "OK";
        if (status == 404) status_msg = "Not Found";
        if (status == 400) status_msg = "Bad Request";
        if (status == 204) status_msg = "No Content";

        std::stringstream response;
        response << "HTTP/1.1 " << status << " " << status_msg << "\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << body.length() << "\r\n";
        response << "Connection: close\r\n";
        response << "Access-Control-Allow-Origin: *\r\n";
        response << "\r\n";
        response << body;

        auto self(shared_from_this());
        asio::async_write(socket_, asio::buffer(response.str()),
            [this, self](std::error_code ec, std::size_t)
            {
                // Connection closed by shared_ptr destruction
            });
    }

    asio::ip::tcp::socket socket_;
    std::shared_ptr<tunnel::Tunnel> tunnel_;
    asio::steady_timer timer_;
    std::array<char, 4096> buffer_;
    int log_sub_id_ = -1;
};

HttpController::HttpController(asio::io_context& io_context, std::string address, int port, std::shared_ptr<tunnel::Tunnel> tunnel)
    : io_context_(io_context), acceptor_(io_context), tunnel_(std::move(tunnel))
{
    
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    
    LOG_INFO("External controller listening at http://%s:%d", address.c_str(), port);
}

void HttpController::start()
{
    do_accept();
}

void HttpController::do_accept()
{
    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket)
        {
            if (!ec)
            {
                std::make_shared<HttpSession>(std::move(socket), tunnel_)->start();
            }
            do_accept();
        });
}

} // namespace control
} // namespace clash
