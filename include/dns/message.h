#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace clash
{
    namespace dns
    {
        /**
         * @brief DNS 头部结构
         */
        struct Header
        {
            uint16_t id;      // 事务 ID
            uint16_t flags;   // 标志位
            uint16_t qdcount; // 问题计数
            uint16_t ancount; // 回答计数
            uint16_t nscount; // 授权记录计数
            uint16_t arcount; // 附加记录计数
        };

        /**
         * @brief DNS 问题部分
         */
        struct Question
        {
            std::string qname; // 查询域名
            uint16_t qtype;    // 查询类型 (A, AAAA, etc.)
            uint16_t qclass;   // 查询类 (IN)
        };

        /**
         * @brief DNS 资源记录 (回答)
         */
        struct ResourceRecord
        {
            std::string name;           // 域名
            uint16_t type;              // 类型
            uint16_t class_;            // 类
            uint32_t ttl;               // 生存时间
            uint16_t rdlength;          // 数据长度
            std::vector<uint8_t> rdata; // 资源数据
        };

        /**
         * @brief DNS 消息类
         * 
         * 负责编码和解码 DNS 数据包。
         */
        class Message
        {
        public:
            Header header;
            std::vector<Question> questions;
            std::vector<ResourceRecord> answers;

            Message();
            
            /**
             * @brief 编码为字节流
             * 
             * @return std::vector<uint8_t> DNS 数据包字节流
             */
            std::vector<uint8_t> encode() const;
            
            /**
             * @brief 从字节流解码
             * 
             * @param buffer 字节流
             * @param msg 输出消息对象
             * @return true 解码成功
             * @return false 解码失败
             */
            static bool decode(const std::vector<uint8_t>& buffer, Message& msg);

            /**
             * @brief 添加查询
             * 
             * @param name 域名
             * @param type 类型
             */
            void addQuestion(const std::string& name, uint16_t type);
        };

    } // namespace dns
} // namespace clash
