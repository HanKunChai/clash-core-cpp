#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include "constant/dns.h"
#include "constant/listener.h"
#include "log/log.h"
#include "tunnel/mode.h"
#include "rule/rule_interface.h"

namespace clash
{
    namespace config
    {
        /**
         * @brief 控制器配置
         */
        struct Controller
        {
            std::string externalController; // 外部控制器地址 (host:port)
            std::string externalUI;         // 外部 UI 路径
            std::string secret;             // API 密钥
        };

        /**
         * @brief 传统入站端口配置
         */
        struct LegacyInbound
        {
            int port = 0;           // HTTP 代理端口
            int socksPort = 0;      // SOCKS5 代理端口
            int redirPort = 0;      // 透明代理端口 (Redir)
            int tproxyPort = 0;     // 透明代理端口 (TProxy)
            int mixedPort = 0;      // 混合端口 (HTTP + SOCKS5)
            bool allowLan = false;  // 是否允许局域网连接
            std::string bindAddress; // 绑定地址
        };

        /**
         * @brief 通用配置
         */
        struct General : public LegacyInbound, public Controller
        {
            std::vector<std::string> authentication; // 认证信息 (user:pass)
            tunnel::Mode mode = tunnel::Mode::Rule;  // 运行模式 (Rule, Global, Direct)
            log::Level logLevel = log::Level::Info;  // 日志级别
            bool ipv6 = false;                       // 是否启用 IPv6
            std::string interfaceName;               // 出口网卡名称
            int routingMark = 0;                     // 路由标记
        };

        /**
         * @brief DNS 回退过滤器
         */
        struct FallbackFilter
        {
            bool geoip = false;              // 是否启用 GeoIP 检查
            std::string geoipCode;           // GeoIP 代码 (如 CN)
            std::vector<std::string> ipcidr; // IP CIDR 列表
            std::vector<std::string> domain; // 域名列表
        };

        /**
         * @brief DNS 配置
         */
        struct DNS
        {
            bool enable = false;             // 是否启用内置 DNS
            bool ipv6 = false;               // 是否解析 IPv6
            std::vector<std::string> nameServer; // 主要 DNS 服务器
            std::vector<std::string> fallback;   // 回退 DNS 服务器
            FallbackFilter fallbackFilter;       // 回退过滤器
            std::string listen;                  // DNS 监听地址
            constant::DNSMode enhancedMode = constant::DNSMode::DNSNormal; // 增强模式 (FakeIP, Redir-Host)
            std::vector<std::string> defaultNameserver; // 默认 DNS 服务器 (用于解析 DoH 域名等)
            std::string fakeIPRange;             // FakeIP 网段
            std::map<std::string, std::string> hosts; // 静态 Hosts
            std::map<std::string, std::string> nameServerPolicy; // 特定域名的 DNS 策略
            std::vector<std::string> searchDomains; // 搜索域
        };

        /**
         * @brief 配置文件存储选项
         */
        struct Profile
        {
            bool storeSelected = false; // 是否存储选择的节点
            bool storeFakeIP = false;   // 是否存储 FakeIP 映射
        };

        /**
         * @brief 实验性功能配置
         */
        struct Experimental
        {
            bool udpFallbackMatch = false;
        };

        /**
         * @brief 代理组配置
         */
        struct ProxyGroup
        {
            std::string name;
            std::string type;
            std::vector<std::string> proxies;
            std::string url;
            int interval = 0;
            std::string strategy;
        };

        /**
         * @brief 完整配置结构
         */
        struct Config
        {
            General general;
            DNS dns;
            Experimental experimental;
            std::map<std::string, std::string> hosts;
            Profile profile;
            std::vector<constant::Inbound> inbounds;
            
            // 代理节点配置 (原始 Map 结构)
            std::vector<std::map<std::string, std::string>> proxies;
            // 代理组配置
            std::vector<ProxyGroup> proxyGroups;
            
            // 路由规则列表
            std::vector<std::shared_ptr<rule::Rule>> rules;

            /**
             * @brief 加载配置文件
             * 
             * @param path 配置文件路径
             * @return Config 配置对象
             */
            static Config load(const std::string& path);
        };

    } // namespace config
} // namespace clash
