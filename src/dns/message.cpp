#include "dns/message.h"
#include <cstring>
#ifndef _WIN32
#include <arpa/inet.h> // for htons, ntohs
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <iostream>

namespace clash
{
    namespace dns
    {
        // 构造函数
        // 初始化 DNS 消息头
        Message::Message()
        {
            std::memset(&header, 0, sizeof(Header));
        }

        // 添加查询问题
        // 将域名和类型添加到问题列表中，并更新头部计数
        void Message::addQuestion(const std::string& name, uint16_t type)
        {
            Question q;
            q.qname = name;
            q.qtype = type;
            q.qclass = 1; // IN (Internet)
            questions.push_back(q);
            header.qdcount++;
        }

        // 辅助函数：编码域名
        // 将点分十进制域名转换为 DNS 格式 (例如: www.google.com -> 3www6google3com0)
        static void encodeName(const std::string& name, std::vector<uint8_t>& buffer)
        {
            size_t start = 0;
            size_t end = 0;
            while ((end = name.find('.', start)) != std::string::npos)
            {
                uint8_t len = static_cast<uint8_t>(end - start);
                buffer.push_back(len);
                for (size_t i = start; i < end; ++i)
                {
                    buffer.push_back(name[i]);
                }
                start = end + 1;
            }
            if (start < name.length())
            {
                uint8_t len = static_cast<uint8_t>(name.length() - start);
                buffer.push_back(len);
                for (size_t i = start; i < name.length(); ++i)
                {
                    buffer.push_back(name[i]);
                }
            }
            buffer.push_back(0); // 根标签
        }

        // 辅助函数：解码域名
        // 从缓冲区中读取 DNS 格式的域名，支持压缩指针
        static std::string decodeName(const std::vector<uint8_t>& buffer, size_t& offset)
        {
            std::string name;
            bool jumped = false;
            size_t jump_offset = 0;
            size_t initial_offset = offset;
            int loops = 0;

            while (true)
            {
                if (loops++ > 10)
                {
                    return ""; // 防止死循环 (例如指针指向自己)
                }
                if (offset >= buffer.size())
                {
                    return "";
                }

                uint8_t len = buffer[offset];

                // 处理压缩指针 (11xxxxxx xxxxxxxx)
                // 如果高两位是 11，表示这是一个指针，指向消息中的另一个位置
                if ((len & 0xC0) == 0xC0)
                {
                    if (offset + 1 >= buffer.size())
                    {
                        return "";
                    }
                    size_t ptr = ((len & 0x3F) << 8) | buffer[offset + 1];
                    if (!jumped)
                    {
                        // 第一次跳转时，记录跳转后的下一个位置，以便函数返回时更新 offset
                        jump_offset = offset + 2;
                        jumped = true;
                    }
                    offset = ptr;
                    continue;
                }

                offset++;

                if (len == 0)
                {
                    break; // 遇到 0 长度标签，结束
                }

                if (!name.empty())
                {
                    name += ".";
                }

                if (offset + len > buffer.size())
                {
                    return "";
                }
                for (int i = 0; i < len; ++i)
                {
                    name += (char)buffer[offset + i];
                }
                offset += len;
            }

            if (jumped)
            {
                offset = jump_offset;
            }

            return name;
        }

        // 编码 DNS 消息
        // 将 Message 对象序列化为字节流，用于网络发送
        std::vector<uint8_t> Message::encode() const
        {
            std::vector<uint8_t> buffer;

            // 转换头部字段为网络字节序 (Big Endian)
            uint16_t id = htons(header.id);
            uint16_t flags = htons(header.flags);
            uint16_t qdcount = htons(header.qdcount);
            uint16_t ancount = htons(header.ancount);
            uint16_t nscount = htons(header.nscount);
            uint16_t arcount = htons(header.arcount);

            // 辅助 lambda：将 uint16_t 按字节写入缓冲区
            // 注意：这里假设输入 val 已经是网络字节序
            auto push_u16_be = [&](uint16_t val)
            {
                const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
                buffer.push_back(p[0]);
                buffer.push_back(p[1]);
            };

            push_u16_be(id);
            push_u16_be(flags);
            push_u16_be(qdcount);
            push_u16_be(ancount);
            push_u16_be(nscount);
            push_u16_be(arcount);

            // 编码问题部分
            for (const auto& q : questions)
            {
                encodeName(q.qname, buffer);
                push_u16_be(htons(q.qtype));
                push_u16_be(htons(q.qclass));
            }

            return buffer;
        }

        // 解码 DNS 消息
        // 从字节流解析出 Message 对象
        bool Message::decode(const std::vector<uint8_t>& buffer, Message& msg)
        {
            if (buffer.size() < 12)
            {
                return false; // 头部至少 12 字节
            }

            size_t offset = 0;

            // 辅助 lambda：读取 uint16_t (Big Endian)
            auto read_u16 = [&](size_t& off) -> uint16_t
            {
                uint16_t val = (buffer[off] << 8) | buffer[off + 1];
                off += 2;
                return val;
            };

            // 辅助 lambda：读取 uint32_t (Big Endian)
            auto read_u32 = [&](size_t& off) -> uint32_t
            {
                uint32_t val = (buffer[off] << 24) | (buffer[off + 1] << 16) | (buffer[off + 2] << 8) | buffer[off + 3];
                off += 4;
                return val;
            };

            // 解析头部
            msg.header.id = read_u16(offset);
            msg.header.flags = read_u16(offset);
            msg.header.qdcount = read_u16(offset);
            msg.header.ancount = read_u16(offset);
            msg.header.nscount = read_u16(offset);
            msg.header.arcount = read_u16(offset);

            // 解析问题部分
            for (int i = 0; i < msg.header.qdcount; ++i)
            {
                Question q;
                q.qname = decodeName(buffer, offset);
                if (offset + 4 > buffer.size())
                {
                    return false;
                }
                q.qtype = read_u16(offset);
                q.qclass = read_u16(offset);
                msg.questions.push_back(q);
            }

            // 解析回答部分
            for (int i = 0; i < msg.header.ancount; ++i)
            {
                ResourceRecord rr;
                rr.name = decodeName(buffer, offset);
                if (offset + 10 > buffer.size())
                {
                    return false;
                }
                rr.type = read_u16(offset);
                rr.class_ = read_u16(offset);
                rr.ttl = read_u32(offset);
                rr.rdlength = read_u16(offset);

                if (offset + rr.rdlength > buffer.size())
                {
                    return false;
                }
                rr.rdata.assign(buffer.begin() + offset, buffer.begin() + offset + rr.rdlength);
                offset += rr.rdlength;

                msg.answers.push_back(rr);
            }

            return true;
        }

    } // namespace dns
} // namespace clash

