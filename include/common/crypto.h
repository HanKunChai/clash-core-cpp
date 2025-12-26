#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace clash
{
    namespace common
    {
        namespace crypto
        {
            /**
             * @brief 加密算法类型
             */
            enum class CipherType
            {
                AES_128_GCM,
                AES_256_GCM,
                CHACHA20_POLY1305,
                UNKNOWN
            };

            /**
             * @brief AEAD 加密工具类
             * 
             * 提供 AEAD (Authenticated Encryption with Associated Data) 加密和解密功能。
             */
            class AeadCipher
            {
            public:
                /**
                 * @brief 获取密钥长度
                 * 
                 * @param type 算法类型
                 * @return size_t 密钥长度（字节）
                 */
                static size_t KeySize(CipherType type);

                /**
                 * @brief 获取盐长度 (Salt)
                 * 
                 * @param type 算法类型
                 * @return size_t 盐长度（字节）
                 */
                static size_t SaltSize(CipherType type);

                /**
                 * @brief 获取 Nonce 长度
                 * 
                 * @param type 算法类型
                 * @return size_t Nonce 长度（字节）
                 */
                static size_t NonceSize(CipherType type);

                /**
                 * @brief 获取 Tag 长度
                 * 
                 * @param type 算法类型
                 * @return size_t Tag 长度（字节）
                 */
                static size_t TagSize(CipherType type);

                /**
                 * @brief AEAD 加密
                 * 
                 * @param type 算法类型
                 * @param key 密钥
                 * @param iv 初始化向量 (Nonce)
                 * @param plaintext 明文
                 * @param ciphertext 输出密文 (会调整大小)
                 * @param tag 输出认证标签 (会调整大小)
                 * @return true 加密成功
                 * @return false 加密失败
                 */
                static bool encrypt(CipherType type, const std::string& key, const std::string& iv, 
                                    const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& tag);

                /**
                 * @brief AEAD 解密
                 * 
                 * @param type 算法类型
                 * @param key 密钥
                 * @param iv 初始化向量 (Nonce)
                 * @param ciphertext 密文
                 * @param tag 认证标签
                 * @param plaintext 输出明文 (会调整大小)
                 * @return true 解密成功
                 * @return false 解密失败
                 */
                static bool decrypt(CipherType type, const std::string& key, const std::string& iv, 
                                    const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& tag, std::vector<uint8_t>& plaintext);

                /**
                 * @brief 密钥派生 (HKDF-SHA1 for Shadowsocks)
                 * 
                 * @param key 原始密钥
                 * @param salt 盐
                 * @param info 信息
                 * @param len 派生长度
                 * @return std::vector<uint8_t> 派生后的密钥
                 */
                static std::vector<uint8_t> hkdf_sha1(const std::string& key, const std::string& salt, const std::string& info, size_t len);

                /**
                 * @brief Legacy Key Derivation (EVP_BytesToKey)
                 * 
                 * @param type 算法类型
                 * @param password 密码
                 * @return std::vector<uint8_t> 派生后的密钥
                 */
                static std::vector<uint8_t> bytes_to_key(CipherType type, const std::string& password);
            };

        } // namespace crypto
    } // namespace common
} // namespace clash
