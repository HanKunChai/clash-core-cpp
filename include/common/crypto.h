#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace clash {
namespace common {
namespace crypto {

enum class CipherType {
    AES_128_GCM,
    AES_256_GCM,
    CHACHA20_POLY1305,
    UNKNOWN
};

class AeadCipher {
public:
    // 获取密钥长度
    static size_t KeySize(CipherType type);
    // 获取盐长度 (Salt)
    static size_t SaltSize(CipherType type);
    // 获取 Nonce 长度
    static size_t NonceSize(CipherType type);
    // 获取 Tag 长度
    static size_t TagSize(CipherType type);

    // AEAD 加密
    // key: 密钥
    // iv: 初始化向量 (Nonce)
    // plaintext: 明文
    // ciphertext: 输出密文 (会调整大小)
    // tag: 输出认证标签 (会调整大小)
    static bool encrypt(CipherType type, const std::string& key, const std::string& iv, 
                        const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& tag);

    // AEAD 解密
    static bool decrypt(CipherType type, const std::string& key, const std::string& iv, 
                        const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& tag, std::vector<uint8_t>& plaintext);

    // 密钥派生 (HKDF-SHA1 for Shadowsocks)
    static std::vector<uint8_t> hkdf_sha1(const std::string& key, const std::string& salt, const std::string& info, size_t len);

    // Legacy Key Derivation (EVP_BytesToKey)
    static std::vector<uint8_t> bytes_to_key(CipherType type, const std::string& password);
};

} // namespace crypto
} // namespace common
} // namespace clash
