#include "adapter/ssr_protocol.h"
#include "log/log.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <openssl/evp.h>

namespace clash
{
    namespace adapter
    {
        namespace ssr
        {
            using namespace common::crypto;

            /**
             * @brief Origin 协议 (原版协议)
             * 
             * 不进行任何额外的封装或加密，直接透传数据。
             */
            class OriginProtocol : public Protocol
            {
            public:
                std::string name() const override
                {
                    return "origin";
                }
                
                void client_pre_encrypt(const std::vector<uint8_t>& plain, std::vector<uint8_t>& out) override
                {
                    out.insert(out.end(), plain.begin(), plain.end());
                }
                
                void client_post_decrypt(const std::vector<uint8_t>& cipher, std::vector<uint8_t>& out) override
                {
                    out.insert(out.end(), cipher.begin(), cipher.end());
                }
            };

            /**
             * @brief Plain 混淆 (无混淆)
             * 
             * 不进行任何混淆处理，直接透传数据。
             */
            class PlainObfs : public Obfs
            {
            public:
                std::string name() const override
                {
                    return "plain";
                }
                
                std::vector<uint8_t> client_encode(const std::vector<uint8_t>& data) override
                {
                    return data;
                }
                
                std::vector<uint8_t> client_decode(const std::vector<uint8_t>& data) override
                {
                    return data;
                }
            };

            /**
             * @brief HttpSimple 混淆 (HTTP 伪装)
             * 
             * 在连接建立后的第一个包前添加 HTTP 请求头，伪装成 HTTP 流量。
             */
            class HttpSimpleObfs : public Obfs
            {
            public:
                HttpSimpleObfs(const ServerInfo& server) : server_(server)
                {
                }
                
                std::string name() const override
                {
                    return "http_simple";
                }

                std::vector<uint8_t> client_encode(const std::vector<uint8_t>& data) override
                {
                    if (first_packet_)
                    {
                        first_packet_ = false;
                        std::stringstream ss;
                        std::string host = server_.param.empty() ? server_.host : server_.param;
                        
                        // 构造 HTTP GET 请求头
                        ss << "GET / HTTP/1.1\r\n";
                        ss << "Host: " << host << "\r\n";
                        ss << "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.149 Safari/537.36\r\n";
                        ss << "Accept-Encoding: gzip, deflate\r\n";
                        ss << "Connection: keep-alive\r\n";
                        ss << "Keep-Alive: timeout=120\r\n";
                        ss << "\r\n";
                        
                        std::string header = ss.str();
                        std::vector<uint8_t> out(header.begin(), header.end());
                        out.insert(out.end(), data.begin(), data.end());
                        return out;
                    }
                    return data;
                }

                std::vector<uint8_t> client_decode(const std::vector<uint8_t>& data) override
                {
                    // 客户端通常不需要处理服务器返回的 HTTP 响应头混淆
                    // 因为 SSR 服务器通常只在握手阶段进行混淆
                    return data;
                }

            private:
                ServerInfo server_;
                bool first_packet_ = true;
            };


            /**
             * @brief AuthAes128Sha1 协议
             * 
             * 这是一个带认证和随机长度填充的协议。
             * 特点：
             * 1. 使用 AES-128-CBC 加密协议头。
             * 2. 使用 HMAC-SHA1 进行完整性校验。
             * 3. 支持随机长度填充以防御流量分析。
             */
            class AuthAes128Sha1 : public Protocol
            {
            public:
                AuthAes128Sha1(const ServerInfo& server, const UserInfo& user)
                    : server_(server), user_(user)
                {
                    std::vector<uint8_t> key_vec;
                    
                    // 使用 KDF 从密码派生密钥
                    // 密钥长度应匹配主加密算法的密钥长度 (user_.key.size())
                    size_t key_len = user_.key.size();
                    if (key_len == 0) key_len = 16;
                    
                    key_vec = kdf(user_.password, key_len);

                    // 处理协议参数 (UID:Password)
                    if (!server_.param.empty())
                    {
                        size_t colon = server_.param.find(':');
                        if (colon != std::string::npos)
                        {
                            try { uid_ = std::stoi(server_.param.substr(0, colon)); } catch (...) { uid_ = 0; }
                            std::string pass = server_.param.substr(colon + 1);
                            Hash::sha1(pass, key_vec);
                        }
                        else
                        {
                            try { uid_ = std::stoi(server_.param); } catch (...) { uid_ = 0; }
                        }
                    }
                    user_key_.assign(key_vec.begin(), key_vec.end());
                    
                    // 调试日志
                    std::string key_hex;
                    for(auto b : user_key_) {
                        char buf[3];
                        snprintf(buf, 3, "%02X", b);
                        key_hex += buf;
                    }
                    LOG_DEBUG("SSR AuthAes128Sha1 Init. UID: %u, UserKey: %s, Param: %s", uid_, key_hex.c_str(), server_.param.c_str());
                }

                std::string name() const override { return "auth_aes128_sha1"; }

                /**
                 * @brief 密钥派生函数 (KDF)
                 * 
                 * 使用 MD5 循环迭代生成指定长度的密钥。
                 */
                static std::vector<uint8_t> kdf(const std::string& password, size_t key_len)
                {
                    std::vector<uint8_t> b;
                    std::vector<uint8_t> prev;
                    while (b.size() < key_len)
                    {
                        std::vector<uint8_t> digest;
                        std::string input;
                        input.insert(input.end(), prev.begin(), prev.end());
                        input += password;
                        Hash::md5(input, digest);
                        b.insert(b.end(), digest.begin(), digest.end());
                        prev = digest;
                    }
                    b.resize(key_len);
                    return b;
                }

                /**
                 * @brief 客户端预加密 (打包)
                 * 
                 * 如果是首包，打包认证头。
                 * 否则，按块打包数据。
                 */
                void client_pre_encrypt(const std::vector<uint8_t>& plain, std::vector<uint8_t>& out) override
                {
                    if (first_packet_)
                    {
                        first_packet_ = false;
                        pack_auth_data(plain, out);
                    }
                    else
                    {
                        const size_t chunk_size = 8100;
                        size_t offset = 0;
                        while (offset < plain.size())
                        {
                            size_t len = std::min(chunk_size, plain.size() - offset);
                            std::vector<uint8_t> chunk(plain.begin() + offset, plain.begin() + offset + len);
                            pack_data(chunk, plain.size(), out);
                            offset += len;
                        }
                    }
                }

                /**
                 * @brief 客户端后解密 (解包)
                 * 
                 * 验证 HMAC，解析长度，提取有效载荷。
                 */
                void client_post_decrypt(const std::vector<uint8_t>& cipher, std::vector<uint8_t>& out) override
                {
                    recv_buffer_.insert(recv_buffer_.end(), cipher.begin(), cipher.end());
                    
                    while (true)
                    {
                        if (recv_buffer_.size() < 4) break;
                        
                        // 1. 读取长度 (2 bytes LE)
                        uint16_t packed_len = recv_buffer_[0] | (recv_buffer_[1] << 8);
                        
                        // 安全检查：最大包长度
                        if (packed_len >= 8192) 
                        { 
                             LOG_WARN("SSR AuthAes128Sha1 Invalid packet length: %u. Resetting buffer.", packed_len);
                             recv_buffer_.clear();
                             return;
                        }

                        // 2. 验证长度 HMAC
                        std::vector<uint8_t> hmac_key = user_key_;
                        uint32_t pid = recv_id_;
                        hmac_key.push_back(pid & 0xFF);
                        hmac_key.push_back((pid >> 8) & 0xFF);
                        hmac_key.push_back((pid >> 16) & 0xFF);
                        hmac_key.push_back((pid >> 24) & 0xFF);
                        
                        std::vector<uint8_t> len_data = {recv_buffer_[0], recv_buffer_[1]};
                        std::vector<uint8_t> len_mac;
                        Hash::hmac_sha1(std::string(hmac_key.begin(), hmac_key.end()), len_data, len_mac);
                        
                        if (len_mac[0] != recv_buffer_[2] || len_mac[1] != recv_buffer_[3])
                        {
                            LOG_ERROR("SSR AuthAes128Sha1 Length HMAC mismatch. RecvID: %u", recv_id_);
                            // 长度校验失败，无法恢复，清空缓冲区
                            recv_buffer_.clear(); 
                            return;
                        }
                        
                        if (recv_buffer_.size() < packed_len) break; // 等待更多数据
                        
                        // 3. 验证包体 HMAC
                        size_t start_idx = packed_len - 4;
                        std::vector<uint8_t> pkt_mac_calc;
                        std::vector<uint8_t> mac_input(recv_buffer_.begin(), recv_buffer_.begin() + start_idx);
                        Hash::hmac_sha1(std::string(hmac_key.begin(), hmac_key.end()), mac_input, pkt_mac_calc);
                        
                        if (pkt_mac_calc[0] != recv_buffer_[start_idx] ||
                            pkt_mac_calc[1] != recv_buffer_[start_idx+1] ||
                            pkt_mac_calc[2] != recv_buffer_[start_idx+2] ||
                            pkt_mac_calc[3] != recv_buffer_[start_idx+3])
                        {
                            LOG_ERROR("SSR AuthAes128Sha1 Body HMAC mismatch. RecvID: %u", recv_id_);
                            recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + packed_len);
                            continue;
                        }
                        
                        // 4. 提取有效载荷 (Payload)
                        size_t header_len = 4; // Length(2) + HMAC(2)
                        size_t rand_len = 0;
                        size_t rand_part_len = 0;
                        
                        // 解析随机数据长度
                        if (recv_buffer_[header_len] == 0xFF)
                        {
                            uint16_t rl = recv_buffer_[header_len+1] | (recv_buffer_[header_len+2] << 8);
                            rand_len = rl - 3;
                            rand_part_len = 3;
                        }
                        else
                        {
                            rand_len = recv_buffer_[header_len] - 1;
                            rand_part_len = 1;
                        }
                        
                        size_t data_start = header_len + rand_part_len + rand_len;
                        if (data_start > packed_len - 4) 
                        {
                             LOG_ERROR("SSR AuthAes128Sha1 Invalid data start index");
                             recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + packed_len);
                             continue;
                        }

                        size_t data_len = packed_len - 4 - data_start;
                        
                        if (data_len > 0)
                        {
                            out.insert(out.end(), 
                                       recv_buffer_.begin() + data_start, 
                                       recv_buffer_.begin() + data_start + data_len);
                        }
                        
                        recv_id_++;
                        recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + packed_len);
                    }
                }

            private:
                /**
                 * @brief 打包首包 (带认证信息)
                 */
                void pack_auth_data(const std::vector<uint8_t>& data, std::vector<uint8_t>& out)
                {
                    size_t data_len = data.size();
                    int rand_len = (data_len > 400) ? (rand() % 512) : (rand() % 1024);
                    
                    // 结构: Head(1) + HeadMAC(6) + UID(4) + EncHeader(16) + HeadHMAC(4) + Rand + Data + BodyHMAC(4)
                    size_t total_len = 7 + 4 + 16 + 4 + rand_len + data_len + 4;

                    std::vector<uint8_t> key_iv(user_.iv.begin(), user_.iv.end());
                    key_iv.insert(key_iv.end(), user_.key.begin(), user_.key.end());

                    // 1. Random Head (1)
                    uint8_t rand_head = rand() % 256;
                    out.push_back(rand_head);

                    // 2. Head HMAC (6)
                    std::vector<uint8_t> head_mac;
                    Hash::hmac_sha1(std::string(key_iv.begin(), key_iv.end()), {rand_head}, head_mac);
                    out.insert(out.end(), head_mac.begin(), head_mac.begin() + 6);

                    // 3. UID (4)
                    uint32_t uid = uid_;
                    if (server_.param.empty()) uid = rand();
                    out.push_back(uid & 0xFF);
                    out.push_back((uid >> 8) & 0xFF);
                    out.push_back((uid >> 16) & 0xFF);
                    out.push_back((uid >> 24) & 0xFF);

                    // 4. Encrypted Header (16)
                    std::vector<uint8_t> header_plain(16);
                    uint32_t now = time(nullptr);
                    header_plain[0] = now & 0xFF;
                    header_plain[1] = (now >> 8) & 0xFF;
                    header_plain[2] = (now >> 16) & 0xFF;
                    header_plain[3] = (now >> 24) & 0xFF;
                    
                    for(int i=4; i<8; ++i) header_plain[i] = rand() % 256;
                    
                    static std::atomic<uint32_t> global_conn_id{0};
                    uint32_t conn_id = global_conn_id++;
                    header_plain[8] = conn_id & 0xFF;
                    header_plain[9] = (conn_id >> 8) & 0xFF;
                    header_plain[10] = (conn_id >> 16) & 0xFF;
                    header_plain[11] = (conn_id >> 24) & 0xFF;

                    // Little Endian for Lengths in Encrypted Header
                    header_plain[12] = total_len & 0xFF;
                    header_plain[13] = (total_len >> 8) & 0xFF;
                    header_plain[14] = rand_len & 0xFF;
                    header_plain[15] = (rand_len >> 8) & 0xFF;

                    std::vector<uint8_t> enc_header = encrypt_header(header_plain);
                    out.insert(out.end(), enc_header.begin(), enc_header.end());

                    // 5. Header HMAC (4)
                    // Covers UID + Encrypted Header
                    // out currently has: Head(1) + HeadMAC(6) + UID(4) + EncHeader(16)
                    // We need HMAC of out[7:]
                    std::vector<uint8_t> mac_input(out.begin() + 7, out.end());
                    std::vector<uint8_t> header_mac_val;
                    Hash::hmac_sha1(std::string(key_iv.begin(), key_iv.end()), mac_input, header_mac_val);
                    out.insert(out.end(), header_mac_val.begin(), header_mac_val.begin() + 4);

                    // 6. Random Data
                    for(int i=0; i<rand_len; ++i) out.push_back(rand() % 256);

                    // 7. Data
                    out.insert(out.end(), data.begin(), data.end());

                    // 8. Body HMAC (4)
                    // Covers entire packet
                    std::vector<uint8_t> body_mac;
                    Hash::hmac_sha1(std::string(user_key_.begin(), user_key_.end()), out, body_mac);
                    out.insert(out.end(), body_mac.begin(), body_mac.begin() + 4);
                }

                /**
                 * @brief 打包普通数据包
                 */
                void pack_data(const std::vector<uint8_t>& data, size_t full_len, std::vector<uint8_t>& out)
                {
                    size_t data_len = data.size();
                    int rand_len = get_rand_len(data_len, full_len);
                    
                    size_t packed_len = 2 + 2 + 3 + rand_len + data_len + 4;
                    if (rand_len < 128) packed_len -= 2;

                    std::vector<uint8_t> hmac_key = user_key_;
                    uint32_t pid = pack_id_++;
                    hmac_key.push_back(pid & 0xFF);
                    hmac_key.push_back((pid >> 8) & 0xFF);
                    hmac_key.push_back((pid >> 16) & 0xFF);
                    hmac_key.push_back((pid >> 24) & 0xFF);

                    // 1. Length (2) Little Endian
                    out.push_back(packed_len & 0xFF);
                    out.push_back((packed_len >> 8) & 0xFF);

                    // 2. Length HMAC (2)
                    std::vector<uint8_t> len_data;
                    len_data.push_back(packed_len & 0xFF);
                    len_data.push_back((packed_len >> 8) & 0xFF);
                    std::vector<uint8_t> len_mac;
                    Hash::hmac_sha1(std::string(hmac_key.begin(), hmac_key.end()), len_data, len_mac);
                    out.insert(out.end(), len_mac.begin(), len_mac.begin() + 2);

                    // 3. Random Data Prefix + Random Data
                    if (rand_len < 128) {
                        out.push_back((uint8_t)(rand_len + 1));
                    } else {
                        out.push_back(0xFF);
                        uint16_t rl = rand_len + 3;
                        out.push_back(rl & 0xFF);
                        out.push_back((rl >> 8) & 0xFF);
                    }
                    for(int i=0; i<rand_len; ++i) out.push_back(rand() % 256);

                    // 4. Data
                    out.insert(out.end(), data.begin(), data.end());

                    // 5. Body HMAC (4)
                    // Covers [Length] ... [Data]
                    // Current out size is packed_len - 4
                    // We need to HMAC the last (packed_len - 4) bytes
                    size_t start_idx = out.size() - (packed_len - 4);
                    std::vector<uint8_t> mac_input(out.begin() + start_idx, out.end());
                    std::vector<uint8_t> pkt_mac;
                    Hash::hmac_sha1(std::string(hmac_key.begin(), hmac_key.end()), mac_input, pkt_mac);
                    out.insert(out.end(), pkt_mac.begin(), pkt_mac.begin() + 4);
                }

                int trapezoid_random(int max, double d) 
                {
                    if (max <= 0) return 0;
                    double base = (double)rand() / RAND_MAX;
                    if (d - 0 > 1e-6) 
                    {
                        double a = 1 - d;
                        base = (std::sqrt(a * a + 4 * d * base) - a) / (2 * d);
                    }
                    return (int)(base * max);
                }

                int get_rand_len(int data_len, int full_len) 
                {
                    if (full_len >= 32 * 1024) return 0; 
                    int rev_len = 1460 - data_len - 9;
                    if (rev_len == 0) return 0;
                    if (rev_len < 0) 
                    {
                        if (rev_len > -1460) 
                        {
                            return trapezoid_random(rev_len + 1460, -0.3);
                        }
                        return rand() % 32;
                    }
                    if (data_len > 900) 
                    {
                        return rand() % rev_len;
                    }
                    return trapezoid_random(rev_len, -0.3);
                }

                std::vector<uint8_t> encrypt_header(const std::vector<uint8_t>& plain)
                {
                    std::string salt = "auth_aes128_sha1";
                    std::string b64_key = base64_encode(user_key_);
                    std::string password = b64_key + salt;
                    
                    std::vector<uint8_t> key = kdf(password, 16);
                    std::vector<uint8_t> iv(16, 0);
                    
                    std::vector<uint8_t> out(plain.size() + 16);
                    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
                    EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.data(), iv.data());
                    EVP_CIPHER_CTX_set_padding(ctx, 0); // Disable padding
                    
                    int len = 0, ciphertext_len = 0;
                    EVP_EncryptUpdate(ctx, out.data(), &len, plain.data(), plain.size());
                    ciphertext_len = len;
                    EVP_EncryptFinal_ex(ctx, out.data() + len, &len);
                    ciphertext_len += len;
                    EVP_CIPHER_CTX_free(ctx);
                    
                    out.resize(ciphertext_len);
                    return out;
                }

                std::string base64_encode(const std::vector<uint8_t>& in)
                {
                    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                    std::string out;
                    unsigned int val = 0;
                    int valb = -6;
                    for (uint8_t c : in) 
                    {
                        val = (val << 8) + c;
                        valb += 8;
                        while (valb >= 0) 
                        {
                            out.push_back(chars[(val >> valb) & 0x3F]);
                            valb -= 6;
                        }
                    }
                    if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
                    while (out.size() % 4) out.push_back('=');
                    return out;
                }

                ServerInfo server_;
                UserInfo user_;
                std::vector<uint8_t> user_key_;
                uint32_t uid_ = 0;
                bool first_packet_ = true;
                uint32_t pack_id_ = 1;
                std::vector<uint8_t> recv_buffer_;
                uint32_t recv_id_ = 1;
            };

            
            // --- Factory (工厂模式) ---
            std::shared_ptr<Protocol> Factory::createProtocol(const std::string& name, const ServerInfo& server, const UserInfo& user)
            {
                if (name == "origin")
                {
                    return std::make_shared<OriginProtocol>();
                }
                if (name == "auth_aes128_sha1")
                {
                    return std::make_shared<AuthAes128Sha1>(server, user);
                }
                // TODO: 实现 auth_aes128_md5 等
                LOG_WARN("Unsupported SSR protocol: %s, falling back to origin", name.c_str());
                return std::make_shared<OriginProtocol>();
            }

            std::shared_ptr<Obfs> Factory::createObfs(const std::string& name, const ServerInfo& server, const UserInfo& user)
            {
                if (name == "plain")
                {
                    return std::make_shared<PlainObfs>();
                }
                if (name == "http_simple")
                {
                    return std::make_shared<HttpSimpleObfs>(server);
                }
                // TODO: 实现 tls1.2_ticket_auth 等
                LOG_WARN("Unsupported SSR obfs: %s, falling back to plain", name.c_str());
                return std::make_shared<PlainObfs>();
            }

        }
    }
}
