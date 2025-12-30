#include "adapter/shadowsocksr.h"
#include "log/log.h"
#include "common/connection.h"
#include "common/crypto.h"
#include "dns/resolver.h"
#include <random>
#include <openssl/rand.h>

namespace clash
{
    namespace adapter
    {
        using namespace common::crypto;

        /**
         * @brief ShadowsocksR 连接实现类
         * 
         * 负责处理 SSR 协议的数据传输，包括混淆解码、流解密、协议后解密等。
         * 继承自 common::Connection，提供异步读写接口。
         */
        class SSRConnection : public common::Connection, public std::enable_shared_from_this<SSRConnection>
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param socket 底层 TCP 套接字
             * @param protocol SSR 协议处理器
             * @param obfs SSR 混淆处理器
             * @param cipher_type 加密算法类型
             * @param key 密钥
             * @param encryptor 加密器实例
             */
            SSRConnection(asio::ip::tcp::socket socket, 
                          std::shared_ptr<ssr::Protocol> protocol,
                          std::shared_ptr<ssr::Obfs> obfs,
                          CipherType cipher_type,
                          const std::string& key,
                          std::unique_ptr<StreamCipher> encryptor)
                : socket_(std::move(socket)), protocol_(protocol), obfs_(obfs),
                  cipher_type_(cipher_type), key_(key), encryptor_(std::move(encryptor))
            {
            }

            /**
             * @brief 异步读取数据
             * 
             * 执行 SSR 的解包流程：
             * 1. 混淆解码 (Obfs decode)
             * 2. 流解密 (Stream decrypt)
             * 3. 协议后解密 (Protocol post-decrypt)
             * 
             * @param buffer 用户提供的缓冲区
             * @param handler 完成回调
             */
            void async_read_some(const asio::mutable_buffer& buffer, std::function<void(std::error_code, std::size_t)> handler) override
            {
                auto self(shared_from_this());
                socket_.async_read_some(asio::buffer(read_buffer_),
                    [this, self, buffer, handler](std::error_code ec, std::size_t length) mutable
                    {
                        if (ec)
                        {
                            LOG_ERROR("SSR Read error: %s", ec.message().c_str());
                            handler(ec, 0);
                            return;
                        }

                        LOG_DEBUG("SSR Read raw bytes: %zu", length);

                        std::vector<uint8_t> data(read_buffer_.data(), read_buffer_.data() + length);
                        
                        // 1. 混淆解码 (Obfs decode)
                        // 处理 HTTP/TLS 等伪装
                        data = obfs_->client_decode(data);
                        if (data.empty())
                        {
                            LOG_DEBUG("SSR Obfs decode consumed all data, reading more...");
                            // 需要更多数据，继续读取
                            async_read_some(buffer, handler);
                            return;
                        }

                        // 2. 解密 (Decrypt)
                        // 如果是第一次读取，提取 IV
                        if (!decryptor_)
                        {
                            size_t iv_len = StreamCipher::IvSize(cipher_type_);
                            if (data.size() < iv_len)
                            {
                                LOG_ERROR("SSR Read buffer too small for IV: %zu < %zu", data.size(), iv_len);
                                // 缓冲区 IV... TODO: 处理分片 IV
                                // 目前假设 IV 在第一个包中（通常如此）
                                handler(std::make_error_code(std::errc::message_size), 0); 
                                return;
                            }
                            std::string iv(data.begin(), data.begin() + iv_len);
                            decryptor_ = std::make_unique<StreamCipher>(cipher_type_, key_, iv, false);
                            
                            // 从数据中移除 IV
                            data.erase(data.begin(), data.begin() + iv_len);
                        }

                        std::vector<uint8_t> decrypted;
                        if (!data.empty())
                        {
                            decryptor_->update(data, decrypted);
                        }

                        // 3. 协议后解密 (Protocol post-decrypt)
                        // 处理 auth_aes128_sha1 等协议的校验和解包
                        std::vector<uint8_t> plain;
                        if (!decrypted.empty())
                        {
                            protocol_->client_post_decrypt(decrypted, plain);
                        }

                        if (plain.empty())
                        {
                             LOG_DEBUG("SSR Protocol post-decrypt consumed all data, reading more...");
                             // 协议可能消耗了所有数据（例如认证头）
                             async_read_some(buffer, handler);
                             return;
                        }

                        LOG_DEBUG("SSR Read decrypted payload: %zu", plain.size());

                        // 复制到用户缓冲区
                        size_t copy_len = std::min(plain.size(), buffer.size());
                        std::memcpy(buffer.data(), plain.data(), copy_len);
                        
                        // TODO: 处理剩余数据如果 plain > buffer
                        
                        handler(ec, copy_len);
                    });
            }

            /**
             * @brief 异步写入数据
             * 
             * 执行 SSR 的组包流程：
             * 1. 协议预加密 (Protocol pre-encrypt)
             * 2. 流加密 (Stream encrypt)
             * 3. 混淆编码 (Obfs encode)
             * 
             * @param buffer 要写入的数据
             * @param handler 完成回调
             */
            void async_write(const asio::const_buffer& buffer, std::function<void(std::error_code, std::size_t)> handler) override
            {
                std::vector<uint8_t> plain(static_cast<const uint8_t*>(buffer.data()), 
                                         static_cast<const uint8_t*>(buffer.data()) + buffer.size());
                
                LOG_DEBUG("SSR Write payload: %zu", plain.size());

                // 1. 协议预加密 (Protocol pre-encrypt)
                // 添加协议头、校验和等
                std::vector<uint8_t> pre_encrypted;
                protocol_->client_pre_encrypt(plain, pre_encrypted);

                // 2. 加密 (Encrypt)
                // 使用流加密算法加密
                std::vector<uint8_t> encrypted;
                encryptor_->update(pre_encrypted, encrypted);

                // 3. 混淆编码 (Obfs encode)
                // 添加混淆头
                std::vector<uint8_t> encoded = obfs_->client_encode(encrypted);

                LOG_DEBUG("SSR Write encoded bytes: %zu", encoded.size());

                auto self(shared_from_this());
                asio::async_write(socket_, asio::buffer(encoded),
                    [this, self, handler, buffer_size = buffer.size()](std::error_code ec, std::size_t)
                    {
                        if (ec)
                        {
                            LOG_ERROR("SSR Write error: %s", ec.message().c_str());
                        }
                        // 报告原始缓冲区大小为已写入
                        handler(ec, buffer_size);
                    });
            }

            asio::any_io_executor get_executor() override
            {
                return socket_.get_executor();
            }

            void close() override
            {
                std::error_code ec;
                socket_.close(ec);
            }

        private:
            asio::ip::tcp::socket socket_;
            std::shared_ptr<ssr::Protocol> protocol_;
            std::shared_ptr<ssr::Obfs> obfs_;
            CipherType cipher_type_;
            std::string key_;
            
            std::unique_ptr<StreamCipher> encryptor_;
            std::unique_ptr<StreamCipher> decryptor_;
            
            std::array<char, 8192> read_buffer_;
        };

        /**
         * @brief SSR 拨号器
         * 
         * 负责建立 TCP 连接并执行 SSR 握手流程。
         */
        class SSRDialer : public std::enable_shared_from_this<SSRDialer>
        {
        public:
            SSRDialer(ShadowsocksRAdapter::Option option, constant::Metadata metadata, asio::io_context& io_context, ProxyAdapter::ConnectHandler handler)
                : option_(std::move(option)), metadata_(std::move(metadata)), socket_(io_context), handler_(std::move(handler)), resolver_(io_context)
            {
            }

            /**
             * @brief 开始连接流程
             * 
             * 解析域名 -> 连接 -> 握手
             */
            void start()
            {
                LOG_DEBUG("SSRDialer start resolving: %s:%d", option_.server.c_str(), option_.port);
                auto self = shared_from_this();
                resolver_.async_resolve(option_.server, std::to_string(option_.port),
                    [this, self](const std::error_code& ec, asio::ip::tcp::resolver::results_type results)
                    {
                        if (!ec)
                        {
                            LOG_DEBUG("SSRDialer resolved success");
                            connect(results);
                        }
                        else
                        {
                            LOG_ERROR("SSRDialer resolve error: %s", ec.message().c_str());
                            handler_(ec, nullptr);
                        }
                    });
            }

        private:
            void connect(const asio::ip::tcp::resolver::results_type& endpoints)
            {
                LOG_DEBUG("SSRDialer connecting...");
                auto self = shared_from_this();
                asio::async_connect(socket_, endpoints,
                    [this, self](std::error_code ec, asio::ip::tcp::endpoint)
                    {
                        if (!ec)
                        {
                            LOG_DEBUG("SSRDialer connected");
                            handshake();
                        }
                        else
                        {
                            LOG_ERROR("SSRDialer connect error: %s", ec.message().c_str());
                            handler_(ec, nullptr);
                        }
                    });
            }

            /**
             * @brief 执行 SSR 握手
             * 
             * 1. 派生密钥
             * 2. 生成 IV
             * 3. 构造目标地址包
             * 4. 加密并发送握手包
             */
            void handshake()
            {
                LOG_DEBUG("SSRDialer handshake start");
                // 1. 派生密钥 (Derive Key)
                CipherType cipher_type = CipherType::UNKNOWN;
                if (option_.cipher == "chacha20-ietf")
                {
                    cipher_type = CipherType::CHACHA20_IETF;
                }
                else if (option_.cipher == "aes-128-ctr")
                {
                    cipher_type = CipherType::AES_128_CTR;
                }
                else if (option_.cipher == "aes-256-ctr")
                {
                    cipher_type = CipherType::AES_256_CTR;
                }
                else if (option_.cipher == "rc4-md5")
                {
                    cipher_type = CipherType::RC4_MD5;
                }
                
                if (cipher_type == CipherType::UNKNOWN)
                {
                    LOG_ERROR("Unsupported SSR cipher: %s", option_.cipher.c_str());
                    handler_(std::make_error_code(std::errc::not_supported), nullptr);
                    return;
                }

                // 简单的 MD5 密钥派生 (兼容大多数 SSR)
                // 循环 MD5(prev_block + password)
                std::vector<uint8_t> key_vec;
                Hash::md5(option_.password, key_vec);
                size_t key_len = StreamCipher::KeySize(cipher_type);
                while (key_vec.size() < key_len)
                {
                    std::vector<uint8_t> next;
                    // 仅使用最后 16 字节 (MD5 大小) 作为前一个块
                    size_t prev_len = 16; 
                    if (key_vec.size() < prev_len)
                    {
                        prev_len = key_vec.size();
                    }
                    
                    std::string input(key_vec.end() - prev_len, key_vec.end());
                    input += option_.password;
                    Hash::md5(input, next);
                    key_vec.insert(key_vec.end(), next.begin(), next.end());
                }
                std::string key(key_vec.begin(), key_vec.begin() + key_len);

                // 2. 生成 IV (Generate IV)
                size_t iv_len = StreamCipher::IvSize(cipher_type);
                std::string iv(iv_len, 0);
                if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), iv_len) != 1)
                {
                    handler_(std::make_error_code(std::errc::state_not_recoverable), nullptr);
                    return;
                }

                // 3. 创建协议和混淆 (Create Protocol and Obfs)
                ssr::ServerInfo server{option_.server, option_.port, option_.protocol_param};
                ssr::UserInfo user{option_.password, key, iv};
                
                auto protocol = ssr::Factory::createProtocol(option_.protocol, server, user);
                auto obfs = ssr::Factory::createObfs(option_.obfs, server, user);

                // 4. 准备握手数据 (Prepare Handshake Data)
                // 4.1 目标地址 (Target)
                std::vector<uint8_t> target;
                
                // 确定地址类型
                bool use_domain = !metadata_.host.empty();
                asio::ip::address ip_addr;
                
                if (!use_domain)
                {
                    try
                    {
                        ip_addr = asio::ip::make_address(metadata_.dstIP);
                    }
                    catch (...)
                    {
                        // 如果解析失败，作为域名处理 (回退)
                        use_domain = true;
                    }
                }

                if (use_domain)
                {
                    target.push_back(0x03); // Domain
                    std::string host = metadata_.host.empty() ? metadata_.dstIP : metadata_.host;
                    target.push_back(static_cast<uint8_t>(host.size()));
                    target.insert(target.end(), host.begin(), host.end());
                }
                else
                {
                     if (ip_addr.is_v4())
                     {
                         target.push_back(0x01); // IPv4
                         auto bytes = ip_addr.to_v4().to_bytes();
                         target.insert(target.end(), bytes.begin(), bytes.end());
                     }
                     else
                     {
                         target.push_back(0x04); // IPv6
                         auto bytes = ip_addr.to_v6().to_bytes();
                         target.insert(target.end(), bytes.begin(), bytes.end());
                     }
                }
                
                uint16_t port = htons(metadata_.dstPort);
                target.push_back(port & 0xFF);
                target.push_back((port >> 8) & 0xFF);

                // 调试日志
                std::string target_hex;
                for(auto b : target)
                {
                    char buf[3];
                    snprintf(buf, 3, "%02X", b);
                    target_hex += buf;
                }
                LOG_DEBUG("SSR Handshake Target: %s", target_hex.c_str());

                // 4.2 协议预加密 (Protocol Pre-encrypt)
                std::vector<uint8_t> pre_encrypted;
                protocol->client_pre_encrypt(target, pre_encrypted);

                // 4.3 加密 (IV + Data)
                auto encryptor = std::make_unique<StreamCipher>(cipher_type, key, iv, true);
                std::vector<uint8_t> encrypted;
                std::vector<uint8_t> iv_vec(iv.begin(), iv.end());
                encrypted.insert(encrypted.end(), iv_vec.begin(), iv_vec.end());
                
                std::vector<uint8_t> cipher_data;
                encryptor->update(pre_encrypted, cipher_data);
                encrypted.insert(encrypted.end(), cipher_data.begin(), cipher_data.end());

                // 4.4 混淆编码 (Obfs Encode)
                std::vector<uint8_t> encoded = obfs->client_encode(encrypted);

                // 5. 发送 (Send)
                LOG_DEBUG("SSRDialer sending handshake data: %zu bytes", encoded.size());
                auto self = shared_from_this();
                auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(encoded)); // Keep buffer alive
                
                asio::async_write(socket_, asio::buffer(*buffer),
                    [this, self, buffer, protocol, obfs, cipher_type, key, encryptor = std::move(encryptor)](std::error_code ec, std::size_t) mutable
                    {
                        if (!ec)
                        {
                            LOG_DEBUG("SSRDialer handshake sent success");
                            // 创建连接 (Create Connection)
                            // 注意: encryptor 已移入 lambda，现在移入 connection
                            auto conn = std::make_shared<SSRConnection>(
                                std::move(socket_), 
                                protocol, 
                                obfs, 
                                cipher_type, 
                                key, 
                                std::move(encryptor)
                            );
                            handler_(ec, std::move(conn));
                        }
                        else
                        {
                            handler_(ec, nullptr);
                        }
                    });
            }

            ShadowsocksRAdapter::Option option_;
            constant::Metadata metadata_;
            asio::ip::tcp::socket socket_;
            ProxyAdapter::ConnectHandler handler_;
            asio::ip::tcp::resolver resolver_;
        };

        ShadowsocksRAdapter::ShadowsocksRAdapter(Option option)
            : option_(std::move(option))
        {
        }

        std::string ShadowsocksRAdapter::name() const
        {
            return option_.name;
        }

        constant::AdapterType ShadowsocksRAdapter::type() const
        {
            return constant::AdapterType::ShadowsocksR;
        }

        void ShadowsocksRAdapter::dial(const constant::Metadata& metadata, asio::io_context& io_context, ConnectHandler handler)
        {
            std::make_shared<SSRDialer>(option_, metadata, io_context, handler)->start();
        }
    }
}
