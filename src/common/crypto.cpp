#include "common/crypto.h"
#include "log/log.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <stdexcept>
#include <cstring>

namespace clash {
namespace common {
namespace crypto {

static const EVP_CIPHER* get_evp_cipher(CipherType type) {
    switch (type) {
        case CipherType::AES_128_GCM: return EVP_aes_128_gcm();
        case CipherType::AES_256_GCM: return EVP_aes_256_gcm();
        case CipherType::CHACHA20_POLY1305: return EVP_chacha20_poly1305();
        default: return nullptr;
    }
}

size_t AeadCipher::KeySize(CipherType type) {
    switch (type) {
        case CipherType::AES_128_GCM: return 16;
        case CipherType::AES_256_GCM: return 32;
        case CipherType::CHACHA20_POLY1305: return 32;
        default: return 0;
    }
}

size_t AeadCipher::SaltSize(CipherType type) {
    return KeySize(type); // Shadowsocks standard: salt size equals key size
}

size_t AeadCipher::NonceSize(CipherType type) {
    switch (type) {
        case CipherType::AES_128_GCM: return 12;
        case CipherType::AES_256_GCM: return 12;
        case CipherType::CHACHA20_POLY1305: return 12;
        default: return 0;
    }
}

size_t AeadCipher::TagSize(CipherType type) {
    return 16; // Standard AEAD tag size
}

bool AeadCipher::encrypt(CipherType type, const std::string& key, const std::string& iv, 
                        const std::vector<uint8_t>& plaintext, std::vector<uint8_t>& ciphertext, std::vector<uint8_t>& tag) {
    const EVP_CIPHER* cipher = get_evp_cipher(type);
    if (!cipher) return false;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool success = false;
    int len;
    int ciphertext_len;

    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) goto end;
    
    // Set IV length if not default (12 bytes is default for GCM, but good to be explicit)
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, iv.length(), nullptr) != 1) goto end;

    // Initialise key and IV
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, 
        reinterpret_cast<const unsigned char*>(key.data()), 
        reinterpret_cast<const unsigned char*>(iv.data())) != 1) goto end;

    // Provide plaintext
    ciphertext.resize(plaintext.size() + EVP_CIPHER_block_size(cipher));
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) goto end;
    ciphertext_len = len;

    // Finalize
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) goto end;
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    // Get Tag
    tag.resize(TagSize(type));
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag.size(), tag.data()) != 1) goto end;

    success = true;

end:
    EVP_CIPHER_CTX_free(ctx);
    return success;
}

bool AeadCipher::decrypt(CipherType type, const std::string& key, const std::string& iv, 
                        const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& tag, std::vector<uint8_t>& plaintext) {
    const EVP_CIPHER* cipher = get_evp_cipher(type);
    if (!cipher) return false;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool success = false;
    int len;
    int plaintext_len;

    // Initialize decryption
    if (EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr) != 1) goto end;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, iv.length(), nullptr) != 1) goto end;

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, 
        reinterpret_cast<const unsigned char*>(key.data()), 
        reinterpret_cast<const unsigned char*>(iv.data())) != 1) goto end;

    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, tag.size(), const_cast<unsigned char*>(tag.data())) != 1) goto end;

    // Provide ciphertext
    plaintext.resize(ciphertext.size() + EVP_CIPHER_block_size(cipher));
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) goto end;
    plaintext_len = len;

    // Finalize (verifies tag)
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        // Verification failed
        goto end;
    }
    plaintext_len += len;
    plaintext.resize(plaintext_len);

    success = true;

end:
    EVP_CIPHER_CTX_free(ctx);
    return success;
}

// HKDF-SHA1 implementation
std::vector<uint8_t> AeadCipher::hkdf_sha1(const std::string& key, const std::string& salt, const std::string& info, size_t len) {
    // 1. Extract
    unsigned char prk[EVP_MAX_MD_SIZE];
    unsigned int prk_len;
    
    HMAC(EVP_sha1(), salt.data(), salt.length(), 
         reinterpret_cast<const unsigned char*>(key.data()), key.length(), 
         prk, &prk_len);

    // 2. Expand
    std::vector<uint8_t> okm;
    okm.reserve(len);
    
    unsigned char t[EVP_MAX_MD_SIZE];
    unsigned int t_len = 0;
    unsigned char c = 1;
    
    while (okm.size() < len) {
        HMAC_CTX* ctx = HMAC_CTX_new();
        HMAC_Init_ex(ctx, prk, prk_len, EVP_sha1(), nullptr);
        
        if (t_len > 0) {
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
}

std::vector<uint8_t> AeadCipher::bytes_to_key(CipherType type, const std::string& password) {
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
