#include "adapter/shadowsocks.h"
#include "log/log.h"
#include "common/connection.h"
#include "common/encrypted_connection.h"
#include "common/crypto.h"
#include <iostream>
#include <arpa/inet.h>

namespace clash
{
    namespace adapter
    {

        // 辅助类：处理 Shadowsocks 连接建立
        // 继承自 enable_shared_from_this 以便在异步回调中安全地使用 this 指针
        class ShadowsocksDialer : public std::enable_shared_from_this<ShadowsocksDialer>
        {
        public:
            ShadowsocksDialer(ShadowsocksAdapter::Option option, constant::Metadata metadata, asio::io_context& io_context, ProxyAdapter::ConnectHandler handler)
                : option_(std::move(option)), metadata_(std::move(metadata)), socket_(io_context), handler_(std::move(handler)), resolver_(io_context)
            {
            }

            // 启动连接流程：首先解析代理服务器的域名
            void start()
            {
                // 1. 解析域名
                auto self = shared_from_this(); // 捕获 self 以保持对象存活
                resolver_.async_resolve(option_.server, std::to_string(option_.port),
                    [this, self](const std::error_code& ec, asio::ip::tcp::resolver::results_type results)
                    {
                        if (!ec)
                        {
                            connect(results);
                        }
                        else
                        {
                            LOG_ERROR("Shadowsocks resolve error: %s", ec.message().c_str());
                            handler_(ec, nullptr);
                        }
                    });
            }

        private:
            // 2. 连接 TCP：连接到解析出的代理服务器 IP 地址
            void connect(const asio::ip::tcp::resolver::results_type& endpoints)
            {
                auto self = shared_from_this();
                asio::async_connect(socket_, endpoints,
                    [this, self](std::error_code ec, asio::ip::tcp::endpoint /*endpoint*/)
                    {
                        if (!ec)
                        {
                            handshake();
                        }
                        else
                        {
                            LOG_ERROR("Shadowsocks connect error: %s", ec.message().c_str());
                            handler_(ec, nullptr);
                        }
                    });
            }

            // 3. 握手 (建立加密连接并发送目标地址)
            // Shadowsocks 协议握手包括：
            // a. 密钥派生 (HKDF 或 BytesToKey)
            // b. 建立加密流 (EncryptedConnection)
            // c. 发送目标地址 (Socks5 风格的地址头)
            void handshake()
            {
                // 解析加密算法
                common::crypto::CipherType cipher_type = common::crypto::CipherType::UNKNOWN;
                if (option_.cipher == "aes-128-gcm")
                {
                    cipher_type = common::crypto::CipherType::AES_128_GCM;
                }
                else if (option_.cipher == "aes-256-gcm")
                {
                    cipher_type = common::crypto::CipherType::AES_256_GCM;
                }
                else if (option_.cipher == "chacha20-ietf-poly1305")
                {
                    cipher_type = common::crypto::CipherType::CHACHA20_POLY1305;
                }

                if (cipher_type == common::crypto::CipherType::UNKNOWN)
                {
                    LOG_ERROR("Unsupported cipher: %s", option_.cipher.c_str());
                    handler_(std::make_error_code(std::errc::not_supported), nullptr);
                    return;
                }

                // 派生密钥 (使用 Legacy EVP_BytesToKey 方式，兼容大多数 SS 实现)
                // 注意：这里使用的是 OpenSSL 的 EVP_BytesToKey，它将密码转换为固定长度的密钥
                std::vector<uint8_t> key_vec = common::crypto::AeadCipher::bytes_to_key(cipher_type, option_.password);
                std::string key(key_vec.begin(), key_vec.end());

                // 创建加密连接包装器
                // EncryptedConnection 会自动处理 Salt 的生成和发送 (在第一次 Write 时)
                // 它包装了底层的 TCP socket，对外提供透明的读写接口
                auto raw_conn = std::make_unique<common::TcpConnection>(std::move(socket_));
                auto conn = std::make_unique<common::EncryptedConnection>(std::move(raw_conn), cipher_type, key, "");

                // 构造目标地址数据包 (Shadowsocks 协议)
                // 格式: [Type][Addr][Port]
                // Type: 1=IPv4, 3=Domain, 4=IPv6
                std::vector<uint8_t> target;

                // 优先使用 Host (Domain) 以避免 DNS 污染
                if (!metadata_.host.empty())
                {
                    target.push_back(3); // Type: Domain
                    target.push_back(static_cast<uint8_t>(metadata_.host.size())); // Length
                    target.insert(target.end(), metadata_.host.begin(), metadata_.host.end()); // Domain
                }
                else
                {
                    // 尝试解析 dstIP
                    struct in_addr ip4;
                    struct in6_addr ip6;

                    if (inet_pton(AF_INET, metadata_.dstIP.c_str(), &ip4) == 1)
                    {
                        target.push_back(1); // Type: IPv4
                        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&ip4);
                        target.insert(target.end(), bytes, bytes + 4);
                    }
                    else if (inet_pton(AF_INET6, metadata_.dstIP.c_str(), &ip6) == 1)
                    {
                        target.push_back(4); // Type: IPv6
                        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&ip6);
                        target.insert(target.end(), bytes, bytes + 16);
                    }
                    else
                    {
                        // 无法识别的 IP，回退到 Domain 模式 (虽然不太可能)
                        target.push_back(3);
                        target.push_back(static_cast<uint8_t>(metadata_.dstIP.size()));
                        target.insert(target.end(), metadata_.dstIP.begin(), metadata_.dstIP.end());
                    }
                }

                // Port (Big Endian)
                // metadata_.dstPort 是 int，需要转换
                uint16_t port = htons(static_cast<uint16_t>(metadata_.dstPort));
                const uint8_t* port_bytes = reinterpret_cast<const uint8_t*>(&port);
                target.push_back(port_bytes[0]);
                target.push_back(port_bytes[1]);

                // 发送目标地址
                // 注意：这里必须使用 conn->async_write，它会负责加密 (包括添加 Salt)
                auto conn_ptr = conn.get();

                // Wrap unique_ptr in a shared_ptr to make it copyable for the lambda capture
                auto conn_holder = std::make_shared<std::unique_ptr<common::Connection>>(std::move(conn));

                conn_ptr->async_write(asio::buffer(target),
                    [this, conn_holder](std::error_code ec, size_t) mutable
                    {
                        if (ec)
                        {
                            LOG_ERROR("Shadowsocks handshake write error: %s", ec.message().c_str());
                            handler_(ec, nullptr);
                        }
                        else
                        {
                            LOG_DEBUG("Shadowsocks handshake success");
                            handler_(std::error_code(), std::move(*conn_holder));
                        }
                    });
            }

            ShadowsocksAdapter::Option option_;
            constant::Metadata metadata_;
            asio::ip::tcp::socket socket_;
            ProxyAdapter::ConnectHandler handler_;
            asio::ip::tcp::resolver resolver_;
        };

        // 构造函数
        ShadowsocksAdapter::ShadowsocksAdapter(Option option)
            : option_(std::move(option))
        {
        }

        std::string ShadowsocksAdapter::name() const
        {
            return option_.name;
        }

        constant::AdapterType ShadowsocksAdapter::type() const
        {
            return constant::AdapterType::Shadowsocks;
        }

        // bool ShadowsocksAdapter::udp() const {
        //     return option_.udp;
        // }

        // 发起连接
        // 创建 ShadowsocksDialer 并启动连接流程
        void ShadowsocksAdapter::dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler)
        {
            std::make_shared<ShadowsocksDialer>(option_, metadata, io_context, std::move(handler))->start();
        }

    } // namespace adapter
} // namespace clash
