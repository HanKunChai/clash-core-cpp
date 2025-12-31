#include "common/encrypted_connection.h"
#include "log/log.h"
#include <openssl/rand.h>
#include <algorithm>
#include <cstring>
#ifndef _WIN32
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace clash
{
namespace common
{

// Helper to encode nonce (little endian)
// Shadowsocks AEAD 协议要求 Nonce 为小端序
static std::string encode_nonce(uint64_t nonce, size_t size)
{
    std::string n(size, 0);
    // Shadowsocks nonce is little-endian
    for (size_t i = 0; i < 8 && i < size; ++i)
    {
        n[i] = (nonce >> (i * 8)) & 0xFF;
    }
    return n;
}

// 构造函数：初始化加密连接
// next: 底层连接 (通常是 TcpConnection)
// type: 加密算法类型
// key: 主密钥 (由密码派生)
// salt: 初始盐值 (如果是服务端，可能已经读取了 Salt；如果是客户端，通常为空)
EncryptedConnection::EncryptedConnection(std::unique_ptr<Connection> next, crypto::CipherType type, const std::string& key, const std::string& salt)
    : next_(std::move(next)), type_(type), key_(key), salt_(salt)
{
    
    // Initialize send salt
    // 生成发送方向的随机 Salt
    size_t salt_len = crypto::AeadCipher::SaltSize(type_);
    send_salt_.resize(salt_len);
    RAND_bytes(reinterpret_cast<unsigned char*>(send_salt_.data()), salt_len);

    // Derive send subkey
    // 使用 HKDF-SHA1 从主密钥和发送 Salt 派生发送子密钥
    send_subkey_ = std::string(reinterpret_cast<const char*>(crypto::AeadCipher::hkdf_sha1(key_, send_salt_, "ss-subkey", crypto::AeadCipher::KeySize(type_)).data()), crypto::AeadCipher::KeySize(type_));

    // If salt is already provided (e.g. server side or pre-negotiated), derive receive subkey
    // 如果构造时提供了接收 Salt (例如服务端已经读取了头部)，则立即派生接收子密钥
    if (!salt_.empty())
    {
        read_state_ = ReadState::Length;
        subkey_ = std::string(reinterpret_cast<const char*>(crypto::AeadCipher::hkdf_sha1(key_, salt_, "ss-subkey", crypto::AeadCipher::KeySize(type_)).data()), crypto::AeadCipher::KeySize(type_));
    }
}

void EncryptedConnection::close()
{
    next_->close();
}

asio::any_io_executor EncryptedConnection::get_executor()
{
    return next_->get_executor();
}

// 异步读取数据
// 这是一个复杂的异步状态机，因为加密数据是分块的，而用户请求读取的长度是任意的。
void EncryptedConnection::async_read_some(const asio::mutable_buffer& buffer, ReadHandler handler)
{
    // 1. 如果解密缓冲区中有剩余数据，直接返回给用户
    if (!decrypted_buffer_.empty())
    {
        size_t len = std::min(buffer.size(), decrypted_buffer_.size());
        std::memcpy(buffer.data(), decrypted_buffer_.data(), len);
        decrypted_buffer_.erase(decrypted_buffer_.begin(), decrypted_buffer_.begin() + len);
        
        // 使用 post 避免回调重入
        asio::post(get_executor(), [handler, len]()
        {
            handler(std::error_code(), len);
        });
        return;
    }

    // 2. 保存用户的回调和缓冲区，启动状态机读取新数据
    user_read_handler_ = std::move(handler);
    user_read_buffer_ = buffer;

    switch (read_state_)
    {
        case ReadState::Salt: do_read_salt(); break;
        case ReadState::Length: do_read_length(); break;
        case ReadState::Payload: do_read_payload(); break;
    }
}

// 状态 1: 读取 Salt
// 仅在连接刚建立且未提供 Salt 时执行
void EncryptedConnection::do_read_salt()
{
    size_t salt_len = crypto::AeadCipher::SaltSize(type_);
    read_buffer_.resize(salt_len);

    auto self = shared_from_this();
    // 使用 async_read 确保读取完整的 Salt
    asio::async_read(*next_, asio::buffer(read_buffer_),
        [this, self](std::error_code ec, size_t)
        {
            if (ec)
            {
                if (user_read_handler_)
                {
                    auto h = std::move(user_read_handler_);
                    h(ec, 0);
                }
                return;
            }

            // 保存 Salt 并派生接收子密钥
            salt_.assign(read_buffer_.begin(), read_buffer_.end());
            subkey_ = std::string(reinterpret_cast<const char*>(crypto::AeadCipher::hkdf_sha1(key_, salt_, "ss-subkey", crypto::AeadCipher::KeySize(type_)).data()), crypto::AeadCipher::KeySize(type_));
            
            // 状态流转 -> 读取长度
            read_state_ = ReadState::Length;
            do_read_length();
        });
}

// 状态 2: 读取加密的长度块
// 长度块包含 2 字节的长度信息 + Tag
void EncryptedConnection::do_read_length()
{
    size_t len_size = 2 + crypto::AeadCipher::TagSize(type_);
    read_buffer_.resize(len_size);

    auto self = shared_from_this();
    asio::async_read(*next_, asio::buffer(read_buffer_),
        [this, self](std::error_code ec, size_t)
        {
            if (ec)
            {
                if (user_read_handler_)
                {
                    auto h = std::move(user_read_handler_);
                    h(ec, 0);
                }
                return;
            }

            // Decrypt Length
            std::vector<uint8_t> ciphertext(read_buffer_.begin(), read_buffer_.begin() + 2);
            std::vector<uint8_t> tag(read_buffer_.begin() + 2, read_buffer_.end());
            std::vector<uint8_t> plaintext;

            if (!crypto::AeadCipher::decrypt(type_, subkey_, encode_nonce(nonce_++, crypto::AeadCipher::NonceSize(type_)), ciphertext, tag, plaintext))
            {
                LOG_ERROR("Shadowsocks decrypt length failed");
                if (user_read_handler_)
                {
                    auto h = std::move(user_read_handler_);
                    h(std::make_error_code(std::errc::bad_message), 0);
                }
                return;
            }

            uint16_t len_be;
            std::memcpy(&len_be, plaintext.data(), 2);
            current_payload_len_ = ntohs(len_be) & 0x3FFF;

            if (current_payload_len_ > 0x3FFF)
            {
                 LOG_ERROR("Shadowsocks invalid payload length: %d", current_payload_len_);
                 if (user_read_handler_)
                 {
                     auto h = std::move(user_read_handler_);
                     h(std::make_error_code(std::errc::value_too_large), 0);
                 }
                 return;
            }

            read_state_ = ReadState::Payload;
            do_read_payload();
        });
}

// 状态 3: 读取加密的负载数据
// 负载数据长度由上一步读取的长度决定
void EncryptedConnection::do_read_payload()
{
    // 计算总读取长度 = 负载长度 + Tag 长度
    size_t total_len = current_payload_len_ + crypto::AeadCipher::TagSize(type_);
    read_buffer_.resize(total_len);

    auto self = shared_from_this();
    asio::async_read(*next_, asio::buffer(read_buffer_),
        [this, self](std::error_code ec, size_t)
        {
            if (ec)
            {
                if (user_read_handler_)
                {
                    auto h = std::move(user_read_handler_);
                    h(ec, 0);
                }
                return;
            }

            // 分离密文和 Tag
            std::vector<uint8_t> ciphertext(read_buffer_.begin(), read_buffer_.begin() + current_payload_len_);
            std::vector<uint8_t> tag(read_buffer_.begin() + current_payload_len_, read_buffer_.end());
            std::vector<uint8_t> plaintext;

            // 解密负载数据
            // 注意：Nonce 在每次解密后自增
            if (!crypto::AeadCipher::decrypt(type_, subkey_, encode_nonce(nonce_++, crypto::AeadCipher::NonceSize(type_)), ciphertext, tag, plaintext))
            {
                LOG_ERROR("Shadowsocks decrypt payload failed");
                if (user_read_handler_)
                {
                    auto h = std::move(user_read_handler_);
                    h(std::make_error_code(std::errc::bad_message), 0);
                }
                return;
            }

            // 将解密后的数据追加到解密缓冲区
            decrypted_buffer_.insert(decrypted_buffer_.end(), plaintext.begin(), plaintext.end());
            
            // 状态流转 -> 回到读取长度状态，准备读取下一个数据块
            read_state_ = ReadState::Length;

            // 如果有挂起的用户读取请求，立即满足
            if (user_read_handler_)
            {
                size_t len = std::min(user_read_buffer_.size(), decrypted_buffer_.size());
                std::memcpy(user_read_buffer_.data(), decrypted_buffer_.data(), len);
                decrypted_buffer_.erase(decrypted_buffer_.begin(), decrypted_buffer_.begin() + len);
                
                auto h = std::move(user_read_handler_);
                h(std::error_code(), len);
            }
        });
}

// 异步写入数据
// Shadowsocks AEAD 写入流程：
// 1. 如果是首次写入，先写入 Salt
// 2. 将数据切分为最大 0x3FFF (16KB - 1) 的块
// 3. 对每个块：
//    a. 加密长度 (2字节) + Tag
//    b. 加密负载 + Tag
void EncryptedConnection::async_write(const asio::const_buffer& buffer, WriteHandler handler)
{
    const uint8_t* data = static_cast<const uint8_t*>(buffer.data());
    size_t size = buffer.size();
    
    std::vector<uint8_t> output;
    
    // 如果是第一次发送，需要在头部附加 Salt
    if (send_nonce_ == 0)
    {
        output.insert(output.end(), send_salt_.begin(), send_salt_.end());
    }

    size_t offset = 0;
    while (offset < size)
    {
        // 计算当前块的大小，最大 16KB - 1
        size_t chunk_len = std::min(size - offset, (size_t)0x3FFF);
        
        // 1. 加密长度部分 (2 字节)
        uint16_t len_be = htons(static_cast<uint16_t>(chunk_len));
        std::vector<uint8_t> len_buf(2);
        std::memcpy(len_buf.data(), &len_be, 2);
        
        std::vector<uint8_t> enc_len, len_tag;
        // 加密长度，Nonce 自增
        crypto::AeadCipher::encrypt(type_, send_subkey_, encode_nonce(send_nonce_++, crypto::AeadCipher::NonceSize(type_)), 
                                    len_buf, enc_len, len_tag);
        
        output.insert(output.end(), enc_len.begin(), enc_len.end());
        output.insert(output.end(), len_tag.begin(), len_tag.end());

        // 2. 加密负载部分
        std::vector<uint8_t> payload(data + offset, data + offset + chunk_len);
        std::vector<uint8_t> enc_payload, payload_tag;
        
        // 加密负载，Nonce 自增
        crypto::AeadCipher::encrypt(type_, send_subkey_, encode_nonce(send_nonce_++, crypto::AeadCipher::NonceSize(type_)), 
                                    payload, enc_payload, payload_tag);
        
        output.insert(output.end(), enc_payload.begin(), enc_payload.end());
        output.insert(output.end(), payload_tag.begin(), payload_tag.end());

        offset += chunk_len;
    }

    // 使用 shared_ptr 管理输出 buffer 的生命周期，确保在异步操作完成前有效
    auto out_ptr = std::make_shared<std::vector<uint8_t>>(std::move(output));
    
    next_->async_write(asio::buffer(*out_ptr), 
        [handler, out_ptr, size](std::error_code ec, std::size_t /*bytes_transferred*/)
        {
            // 回调用户 handler，报告原始数据大小
            handler(ec, size);
        });
}

} // namespace common
} // namespace clash
