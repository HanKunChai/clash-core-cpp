#include "config/config.h"
#include "log/log.h"
#include "rule/domain.h"
#include "rule/domain_keyword.h"
#include "rule/final.h"
#include "rule/ip_cidr.h"
#include "rule/src_ip_cidr.h"
#include "rule/port.h"
#include "rule/process.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <sstream>

namespace clash {
namespace config {

// Helper to safely get values
template<typename T>
T get_optional(const YAML::Node& node, const std::string& key, T default_val) {
    if (node[key]) {
        return node[key].as<T>();
    }
    return default_val;
}

// 辅助函数：解析规则字符串
// 规则格式通常为: TYPE,VALUE,PROXY[,OPTIONS]
// 例如: DOMAIN-SUFFIX,google.com,Proxy
std::shared_ptr<rule::Rule> parseRule(const std::string& ruleStr) {
    std::istringstream iss(ruleStr);
    std::string segment;
    std::vector<std::string> parts;
    
    // 按逗号分割字符串
    while (std::getline(iss, segment, ',')) {
        // 去除首尾空白字符
        segment.erase(0, segment.find_first_not_of(" \t"));
        segment.erase(segment.find_last_not_of(" \t") + 1);
        parts.push_back(segment);
    }

    if (parts.empty()) return nullptr;

    // 规则类型转大写
    std::string type = parts[0];
    std::transform(type.begin(), type.end(), type.begin(), ::toupper);

    // 根据类型创建对应的规则对象
    if (type == "DOMAIN") {
        // 完整域名匹配
        if (parts.size() < 3) return nullptr;
        return std::make_shared<rule::Domain>(parts[1], parts[2]);
    } else if (type == "DOMAIN-SUFFIX") {
        // 域名后缀匹配
        if (parts.size() < 3) return nullptr;
        return std::make_shared<rule::DomainSuffix>(parts[1], parts[2]);
    } else if (type == "DOMAIN-KEYWORD") {
        // 域名关键字匹配
        if (parts.size() < 3) return nullptr;
        return std::make_shared<rule::DomainKeyword>(parts[1], parts[2]);
    } else if (type == "MATCH") {
        // 全匹配 (通常作为最后一条规则)
        if (parts.size() < 2) return nullptr;
        return std::make_shared<rule::Final>(parts[1]);
    } else if (type == "IP-CIDR" || type == "IP-CIDR6") {
        // 目标 IP CIDR 匹配
        if (parts.size() < 3) return nullptr;
        bool noResolve = false;
        // 检查是否有 no-resolve 选项 (不触发 DNS 解析)
        if (parts.size() >= 4 && parts[3] == "no-resolve") {
            noResolve = true;
        }
        return std::make_shared<rule::IPCIDR>(parts[1], parts[2], noResolve);
    } else if (type == "SRC-IP-CIDR") {
        // 源 IP CIDR 匹配
        if (parts.size() < 3) return nullptr;
        return std::make_shared<rule::SrcIPCIDR>(parts[1], parts[2]);
    } else if (type == "SRC-PORT") {
        // 源端口匹配
        if (parts.size() < 3) return nullptr;
        try {
            int port = std::stoi(parts[1]);
            return std::make_shared<rule::Port>(rule::Port::Type::SrcPort, port, parts[2]);
        } catch (...) { return nullptr; }
    } else if (type == "DST-PORT") {
        // 目标端口匹配
        if (parts.size() < 3) return nullptr;
        try {
            int port = std::stoi(parts[1]);
            return std::make_shared<rule::Port>(rule::Port::Type::DstPort, port, parts[2]);
        } catch (...) { return nullptr; }
    } else if (type == "PROCESS-NAME") {
        // 进程名匹配
        if (parts.size() < 3) return nullptr;
        return std::make_shared<rule::Process>(parts[1], parts[2]);
    } else if (type == "PROCESS-PATH") {
        // 进程路径匹配
        if (parts.size() < 3) return nullptr;
        return std::make_shared<rule::ProcessPath>(parts[1], parts[2]);
    }
    
    // TODO: 实现其他规则类型 (如 GEOIP 等)
    log::warn("Unsupported rule type: {}", type);
    return nullptr;
}

// 加载配置文件
// 使用 yaml-cpp 库解析 YAML 格式的配置文件
Config Config::load(const std::string& path) {
    Config cfg;
    
    try {
        YAML::Node config = YAML::LoadFile(path);

        // 1. 解析通用配置 (General)
        cfg.general.port = get_optional(config, "port", 0); // HTTP 代理端口
        cfg.general.socksPort = get_optional(config, "socks-port", 0); // SOCKS5 代理端口
        cfg.general.redirPort = get_optional(config, "redir-port", 0); // 透明代理重定向端口
        cfg.general.tproxyPort = get_optional(config, "tproxy-port", 0); // TProxy 端口
        cfg.general.mixedPort = get_optional(config, "mixed-port", 0); // HTTP/SOCKS5 混合端口
        cfg.general.allowLan = get_optional(config, "allow-lan", false); // 是否允许局域网连接
        cfg.general.bindAddress = get_optional(config, "bind-address", std::string("*")); // 绑定地址
        cfg.general.ipv6 = get_optional(config, "ipv6", false); // 是否启用 IPv6
        cfg.general.interfaceName = get_optional(config, "interface-name", std::string("")); // 指定出口网卡
        
        // 解析运行模式
        std::string modeStr = get_optional(config, "mode", std::string("rule"));
        if (modeStr == "global") cfg.general.mode = tunnel::Mode::Global;
        else if (modeStr == "direct") cfg.general.mode = tunnel::Mode::Direct;
        else if (modeStr == "script") cfg.general.mode = tunnel::Mode::Script;
        else cfg.general.mode = tunnel::Mode::Rule;

        // 解析日志级别
        std::string logLevelStr = get_optional(config, "log-level", std::string("info"));
        if (logLevelStr == "debug") cfg.general.logLevel = log::Level::Debug;
        else if (logLevelStr == "warning") cfg.general.logLevel = log::Level::Warning;
        else if (logLevelStr == "error") cfg.general.logLevel = log::Level::Error;
        else if (logLevelStr == "silent") cfg.general.logLevel = log::Level::Silent;
        else cfg.general.logLevel = log::Level::Info;

        // 外部控制配置 (RESTful API)
        cfg.general.externalController = get_optional(config, "external-controller", std::string(""));
        cfg.general.externalUI = get_optional(config, "external-ui", std::string(""));
        cfg.general.secret = get_optional(config, "secret", std::string(""));

        // 2. 解析 DNS 配置
        if (config["dns"]) {
            YAML::Node dnsNode = config["dns"];
            cfg.dns.enable = get_optional(dnsNode, "enable", false);
            cfg.dns.ipv6 = get_optional(dnsNode, "ipv6", false);
            cfg.dns.listen = get_optional(dnsNode, "listen", std::string(""));
            
            if (dnsNode["nameserver"]) {
                for (const auto& ns : dnsNode["nameserver"]) {
                    cfg.dns.nameServer.push_back(ns.as<std::string>());
                }
            }
            
            if (dnsNode["fallback"]) {
                for (const auto& fb : dnsNode["fallback"]) {
                    cfg.dns.fallback.push_back(fb.as<std::string>());
                }
            }
        }

        // 3. 解析 Profile 配置 (持久化存储)
        if (config["profile"]) {
            cfg.profile.storeSelected = get_optional(config["profile"], "store-selected", false);
            cfg.profile.storeFakeIP = get_optional(config["profile"], "store-fake-ip", false);
        }

        // 4. 解析实验性功能
        if (config["experimental"]) {
            cfg.experimental.udpFallbackMatch = get_optional(config["experimental"], "udp-fallback-match", false);
        }

        // 5. 解析代理节点 (Proxies)
        if (config["proxies"]) {
            for (const auto& proxyNode : config["proxies"]) {
                if (proxyNode.IsMap()) {
                    std::map<std::string, std::string> proxyMap;
                    for (auto it = proxyNode.begin(); it != proxyNode.end(); ++it) {
                        std::string key = it->first.as<std::string>();
                        if (it->second.IsScalar()) {
                            proxyMap[key] = it->second.as<std::string>();
                        }
                    }
                    cfg.proxies.push_back(proxyMap);
                }
            }
        }

        // 6. 解析代理组 (Proxy Groups)
        if (config["proxy-groups"]) {
            for (const auto& groupNode : config["proxy-groups"]) {
                ProxyGroup group;
                group.name = get_optional(groupNode, "name", std::string(""));
                group.type = get_optional(groupNode, "type", std::string("select"));
                group.url = get_optional(groupNode, "url", std::string(""));
                group.interval = get_optional(groupNode, "interval", 0);
                group.strategy = get_optional(groupNode, "strategy", std::string("consistent-hashing"));
                
                if (groupNode["proxies"]) {
                    for (const auto& p : groupNode["proxies"]) {
                        group.proxies.push_back(p.as<std::string>());
                    }
                }
                cfg.proxyGroups.push_back(group);
            }
        }

        // 7. 解析路由规则 (Rules)
        if (config["rules"]) {
            for (const auto& ruleNode : config["rules"]) {
                std::string ruleStr = ruleNode.as<std::string>();
                auto rule = parseRule(ruleStr);
                if (rule) {
                    cfg.rules.push_back(rule);
                }
            }
        }

    } catch (const YAML::Exception& e) {
        log::error("Failed to parse config file: %s", e.what());
        throw; // Re-throw or handle gracefully
    }

    return cfg;
}

} // namespace config
} // namespace clash
