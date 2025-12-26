#pragma once

#include "common/connection.h"
#include "common/crypto.h"
#include <vector>
#include <memory>
#include <deque>

namespace clash
{
    namespace common
    {
        /**
         * @brief 加密连接包装类 (Shadowsocks AEAD 协议实现)
         * 
         * 实现了 Shadowsocks 的 AEAD 加密传输协议。
         * 负责数据的加密发送和解密接收。
         */
        class EncryptedConnection : public Connection, public std::enable_shared_from_this<EncryptedConnection>
        {
        public:
            /**
             * @brief 构造函数
             * 
             * @param next 底层连接
             * @param type 加密类型
             * @param key 密钥
             * @param salt 盐
             */
            EncryptedConnection(std::unique_ptr<Connection> next, crypto::CipherType type, const std::string& key, const std::string& salt);

            void async_read_some(const asio::mutable_buffer& buffer, ReadHandler handler) override;
            void async_write(const asio::const_buffer& buffer, WriteHandler handler) override;
            void close() override;
            asio::any_io_executor get_executor() override;

        private:
            void do_read_salt();
            void do_read_length();
            void do_read_payload();
            
            std::unique_ptr<Connection> next_;
            crypto::CipherType type_;
            std::string key_;
            std::string salt_; // Incoming salt (read from peer)
            std::string send_salt_; // Outgoing salt (generated)
            
            std::string subkey_; // Derived from key + salt
            std::string send_subkey_;

            uint64_t nonce_ = 0;
            uint64_t send_nonce_ = 0;

            // Read state
            enum class ReadState
            {
                Salt,
                Length,
                Payload
            } read_state_ = ReadState::Salt;

            std::vector<uint8_t> read_buffer_;
            std::vector<uint8_t> decrypted_buffer_; // Buffer for decrypted data waiting to be consumed
            
            size_t current_payload_len_ = 0;
            ReadHandler user_read_handler_;
            asio::mutable_buffer user_read_buffer_;

            // Write queue
            struct WriteRequest
            {
                std::vector<uint8_t> data;
                WriteHandler handler;
            };
            std::deque<WriteRequest> write_queue_;
            bool writing_ = false;

            void do_write();
        };

    } // namespace common
} // namespace clash
