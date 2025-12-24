#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace clash {
namespace dns {

// DNS 头部结构
struct Header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

// DNS 问题部分
struct Question {
    std::string qname;
    uint16_t qtype;
    uint16_t qclass;
};

// DNS 资源记录 (回答)
struct ResourceRecord {
    std::string name;
    uint16_t type;
    uint16_t class_;
    uint32_t ttl;
    uint16_t rdlength;
    std::vector<uint8_t> rdata;
};

// DNS 消息类：负责编码和解码 DNS 数据包
class Message {
public:
    Header header;
    std::vector<Question> questions;
    std::vector<ResourceRecord> answers;

    Message();
    
    // 编码为字节流
    std::vector<uint8_t> encode() const;
    
    // 从字节流解码
    static bool decode(const std::vector<uint8_t>& buffer, Message& msg);

    // 辅助：添加查询
    void addQuestion(const std::string& name, uint16_t type);
};

} // namespace dns
} // namespace clash
