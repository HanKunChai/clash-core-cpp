#pragma once
#include <vector>
#include <string>
#include <memory>
#include "common/crypto.h"

namespace clash {
namespace adapter {
namespace ssr {

struct ServerInfo {
    std::string host;
    int port;
    std::string param;
};

struct UserInfo {
    std::string password;
    std::string key; // Derived key
    std::string iv;  // Session IV
};

class Protocol {
public:
    virtual ~Protocol() = default;
    virtual std::string name() const = 0;
    
    // Encapsulate data to be sent (before stream cipher)
    virtual void client_pre_encrypt(const std::vector<uint8_t>& plain, std::vector<uint8_t>& out) = 0;
    
    // Decapsulate received data (after stream cipher decryption)
    virtual void client_post_decrypt(const std::vector<uint8_t>& cipher, std::vector<uint8_t>& out) = 0;
};

class Obfs {
public:
    virtual ~Obfs() = default;
    virtual std::string name() const = 0;

    // Encode data to be sent (after stream cipher encryption)
    virtual std::vector<uint8_t> client_encode(const std::vector<uint8_t>& data) = 0;
    
    // Decode received data (before stream cipher decryption)
    // Returns decoded data, and modifies input to remove consumed bytes if necessary (simplified here)
    virtual std::vector<uint8_t> client_decode(const std::vector<uint8_t>& data) = 0;
};

class Factory {
public:
    static std::shared_ptr<Protocol> createProtocol(const std::string& name, const ServerInfo& server, const UserInfo& user);
    static std::shared_ptr<Obfs> createObfs(const std::string& name, const ServerInfo& server, const UserInfo& user);
};

}
}
}
