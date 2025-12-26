#include "common/crypto.h"
#include "log/log.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/kdf.h>
#include <openssl/params.h>
#endif
#include <stdexcept>
#include <cstring>

namespace clash
{
namespace common
{
namespace crypto
{

// 获取 OpenSSL EVP_CIPHER 结构体
static const EVP_CIPHER* get_evp_cipher(CipherType type)
{
    switch (type)
    {
        case CipherType::AES_128_GCM: return EVP_aes_128_gcm();
        case CipherType::AES_256_GCM: return EVP_aes_256_gcm();
        case CipherType::CHACHA20_POLY1305: return EVP_chacha20_poly1305();
        default: return nullptr;
    }
}

// 获取密钥长度
size_t AeadCipher::KeySize(CipherType type)
{
    switch (type)
    {
        case CipherType::AES_128_GCM: return 16;
        case CipherType::AES_256_GCM: return 32;
        case CipherType::CHACHA20_POLY1305: return 32;
        default: return 0;
    }
}

// 获取盐值长度
// Shadowsocks 标准中，盐值长度等于密钥长度
size_t AeadCipher::SaltSize(CipherType type)
{
    return KeySize(type);
}

// 获取 Nonce (IV) 长度
size_t AeadCipher::NonceSize(CipherType type)
{
    switch (type)
    {
        case CipherType::AES_128_GCM: return 12;
        case CipherType::AES_256_GCM: return 12;
        case CipherType::CHACHA20_POLY1305: return 12;
        default: return 0;
    }
}

// 获取认证标签 (Tag) 长度
// 标准 AEAD 标签长度通常为 16 字节
size_t AeadCipher::TagSize(CipherType type)
{
    return 16;
}

// AEAD 加密
// 输入: 密钥, IV, 明文
// 输出: 密文, 认证标签
bool AeadCipher::encrypt(CipherType type, const std::string& key, const std::string& iv, 
                        const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& tag)
{
    const EVP_CIPHER* cipher = get_evp_cipher(type);
    if (!cipher) return false;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool success = false;
    int len;
    int ciphertext_len;

    // 初始化加密上下文
    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) goto end;
    
    // 设置 IV 长度 (GCM 默认为 12 字节，但显式设置更安全)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, iv.length(), nullptr) != 1) goto end;

    // 初始化密钥和 IV
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, 
        reinterpret_cast<const unsigned char*>(key.data()), 
        reinterpret_cast<const unsigned char*>(iv.data())) != 1) goto end;

    // 处理明文数据
    ciphertext.resize(plaintext.size() + EVP_CIPHER_block_size(cipher));
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) goto end;
    ciphertext_len = len;

    // 结束加密
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) goto end;
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    // 获取认证标签 (Tag)
    tag.resize(TagSize(type));
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag.size(), tag.data()) != 1) goto end;

    success = true;

end:
    EVP_CIPHER_CTX_free(ctx);
    return success;
}

// AEAD 解密
// 输入: 密钥, IV, 密文, 认证标签
// 输出: 明文
bool AeadCipher::decrypt(CipherType type, const std::string& key, const std::string& iv, 
                        const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& tag, std::vector<uint8_t>& plaintext)
{
    const EVP_CIPHER* cipher = get_evp_cipher(type);
    if (!cipher) return false;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool success = false;
    int len;
    int plaintext_len;

    // 初始化解密上下文
    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) goto end;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, iv.length(), nullptr) != 1) goto end;

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, 
        reinterpret_cast<const unsigned char*>(key.data()), 
        reinterpret_cast<const unsigned char*>(iv.data())) != 1) goto end;

    // 设置期望的认证标签 (Tag)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, tag.size(), const_cast<unsigned char*>(tag.data())) != 1) goto end;

    // 处理密文数据
    plaintext.resize(ciphertext.size() + EVP_CIPHER_block_size(cipher));
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) goto end;
    plaintext_len = len;

    // 结束解密 (验证 Tag)
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1)
    {
        // 验证失败
        goto end;
    }
    plaintext_len += len;
    plaintext.resize(plaintext_len);

    success = true;

end:
    EVP_CIPHER_CTX_free(ctx);
    return success;
}

// HKDF-SHA1 实现
// 用于从主密钥派生子密钥
std::vector<uint8_t> AeadCipher::hkdf_sha1(const std::string& key, const std::string& salt, const std::string& info, size_t len)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    // OpenSSL 3.0+ 使用 EVP_KDF API
    std::vector<uint8_t> okm(len);
    EVP_KDF *kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
    if (kdf == NULL)
    {
        throw std::runtime_error("EVP_KDF_fetch failed");
    }
    EVP_KDF_CTX *kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);

    if (kctx == NULL)
    {
        throw std::runtime_error("EVP_KDF_CTX_new failed");
    }

    OSSL_PARAM params[5];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", (char*)"SHA1", 0);
    params[1] = OSSL_PARAM_construct_octet_string("key", (void*)key.data(), key.size());
    params[2] = OSSL_PARAM_construct_octet_string("salt", (void*)salt.data(), salt.size());
    params[3] = OSSL_PARAM_construct_octet_string("info", (void*)info.data(), info.size());
    params[4] = OSSL_PARAM_construct_end();

    if (EVP_KDF_derive(kctx, okm.data(), len, params) <= 0)
    {
        EVP_KDF_CTX_free(kctx);
        throw std::runtime_error("EVP_KDF_derive failed");
    }
    EVP_KDF_CTX_free(kctx);
    return okm;
#else
    // OpenSSL 1.1.x 手动实现 HKDF
    // 1. Extract (提取)
    unsigned char prk[EVP_MAX_MD_SIZE];
    unsigned int prk_len;
    
    HMAC(EVP_sha1(), salt.data(), salt.length(), 
         reinterpret_cast<const unsigned char*>(key.data()), key.length(), 
         prk, &prk_len);

    // 2. Expand (扩展)
    std::vector<uint8_t> okm;
    okm.reserve(len);
    
    unsigned char t[EVP_MAX_MD_SIZE];
    unsigned int t_len = 0;
    unsigned char c = 1;
    
    while (okm.size() < len)
    {
        HMAC_CTX* ctx = HMAC_CTX_new();
        HMAC_Init_ex(ctx, prk, prk_len, EVP_sha1(), nullptr);
        
        if (t_len > 0)
        {
            HMAC_Update(ctx, t, t_len);
        }
        HMAC_Update(ctx, reinterpret_cast<const unsigned char*>(info.data()), info.length());
        HMAC_Update(ctx, &c, 1);
        
        HMAC_Final(ctx, t, &t_len);
        HMAC_CTX_free(ctx);
        
        size_t to_copy = std::min((size_t)t_len, len - okm.size());
        okm.insert(okm.end(), t, t + to_copy);
        c++;
    }
    
    return okm;
#endif
}

// 将密码转换为密钥 (EVP_BytesToKey)
// 兼容旧版 Shadowsocks 的密钥生成方式
std::vector<uint8_t> AeadCipher::bytes_to_key(CipherType type, const std::string& password)
{
    const EVP_CIPHER* cipher = get_evp_cipher(type);
    if (!cipher) return {};

    unsigned char key[EVP_MAX_KEY_LENGTH];
    unsigned char iv[EVP_MAX_IV_LENGTH];
    
    int key_len = EVP_BytesToKey(cipher, EVP_md5(), nullptr, 
        reinterpret_cast<const unsigned char*>(password.data()), password.length(), 
        1, key, iv);
        
    if (key_len <= 0) return {};
    
    return std::vector<uint8_t>(key, key + key_len);
}

} // namespace crypto
} // namespace common
} // namespace clash
