#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <ctime>
#include <cstdlib>
#include "constant/version.h"
#include "constant/path.h"
#include "common/geoip.h"
#include "log/log.h"
#include "config/config.h"
#include "listener/tcp_listener.h"
#include "tunnel/tunnel.h"
#include "adapter/direct.h"
#include "adapter/reject.h"
#include "adapter/socks5.h"
#include "adapter/shadowsocks.h"
#include "adapter/shadowsocksr.h"
#include "adapter/vmess.h"
#include "adapter/selector.h"
#include "adapter/url_test.h"
#include "adapter/fallback.h"
#include "adapter/load_balance.h"
#include "dns/resolver.h"
#include "control/http_controller.h"
#include <asio.hpp>

/**
 * @brief 应用程序入口点
 * 
 * 负责初始化配置、日志、隧道、监听器和控制器，并启动事件循环。
 * 
 * @param argc 参数数量
 * @param argv 参数列表
 * @return int 退出代码
 */
int main(int argc, char* argv[])
{
    using namespace clash;

    // 初始化日志
    log::init();

    // 初始化随机数种子
    std::srand(std::time(nullptr));

    // 检查命令行参数
    if (argc > 1)
    {
        std::string arg = argv[1];
        if (arg == "-v" || arg == "--version")
        {
            std::cout << "Clash Core C++ Port" << std::endl;
            std::cout << "Version: " << constant::Version << std::endl;
            std::cout << "Build Time: " << constant::BuildTime << std::endl;
            std::cout << "Home Dir: " << constant::Path::instance().getHomeDir() << std::endl;
            return 0;
        }
    }

    // 打印版本和环境信息
    std::cout << "Clash Core C++ Port" << std::endl;
    std::cout << "Version: " << constant::Version << std::endl;
    std::cout << "Build Time: " << constant::BuildTime << std::endl;
    std::cout << "Home Dir: " << constant::Path::instance().getHomeDir() << std::endl;

    // 初始化日志系统，默认 Debug 级别
    log::setLevel(log::Level::Debug);
    LOG_INFOLN("Initializing...");
    LOG_DEBUG("Debug mode enabled");

    try
    {
        // 解析配置文件路径
        std::string configPath = constant::Path::instance().resolve("config.yaml");
        LOG_INFO("Loading config from: %s", configPath.c_str());

        // 初始化 GeoIP 数据库
        std::string mmdbPath = constant::Path::instance().getMMDBPath();
        if (std::filesystem::exists(mmdbPath))
        {
            common::GeoIP::instance().init(mmdbPath);
        }
        else
        {
            LOG_WARN("GeoIP database not found at: %s", mmdbPath.c_str());
        }
        
        // 检查配置文件是否存在，如果不存在则创建默认配置 (用于测试)
        std::ifstream f(configPath);
        if (!f.good())
        {
            LOG_WARN("Config file not found, creating a default one for testing...");
            
            // 确保目录存在
            std::filesystem::path p(configPath);
            std::filesystem::create_directories(p.parent_path());

            // 写入默认配置内容
            std::ofstream out(configPath);
            if (out.is_open())
            {
                out << "port: 7890\n"
                    << "socks-port: 7891\n"
                    << "external-controller: 0.0.0.0:9090\n"
                    << "log-level: debug\n"
                    << "mode: rule\n"
                    << "proxies:\n"
                    << "  - name: Proxy\n"
                    << "    type: socks5\n"
                    << "    server: 127.0.0.1\n"
                    << "    port: 1080\n"
                    << "proxy-groups:\n"
                    << "  - name: ProxyGroup\n"
                    << "    type: select\n"
                    << "    proxies:\n"
                    << "      - Proxy\n"
                    << "      - DIRECT\n"
                    << "  - name: AutoGroup\n"
                    << "    type: url-test\n"
                    << "    url: http://www.gstatic.com/generate_204\n"
                    << "    interval: 300\n"
                    << "    proxies:\n"
                    << "      - Proxy\n"
                    << "      - DIRECT\n"
                    << "rules:\n"
                    << "  - DOMAIN-SUFFIX,google.com,AutoGroup\n"
                    << "  - DOMAIN-SUFFIX,facebook.com,REJECT\n"
                    << "  - MATCH,DIRECT\n";
                out.close();
            }
            else
            {
                LOG_ERROR("Failed to create config file at: %s", configPath.c_str());
            }
        }

        // 加载并解析配置文件
        config::Config cfg = config::Config::load(configPath);
        
        // 应用日志级别配置
        log::setLevel(cfg.general.logLevel);

        LOG_INFO("HTTP Port: %d", cfg.general.port);
        LOG_INFO("Socks Port: %d", cfg.general.socksPort);
        LOG_INFO("Log Level: %s", (cfg.general.logLevel == log::Level::Debug ? "Debug" : "Info"));

        // 创建全局 IO Context，用于驱动所有异步操作
        asio::io_context io_context;

        // 初始化 DNS 解析器
        auto resolver = std::make_shared<dns::Resolver>(io_context);
        if (cfg.dns.enable)
        {
            resolver->setNameServers(cfg.dns.nameServer);
        }
        resolver->setHosts(cfg.hosts);
        // 合并 DNS 特定的 Hosts 配置
        if (!cfg.dns.hosts.empty())
        {
            auto hosts = cfg.hosts;
            for (const auto& [k, v] : cfg.dns.hosts)
            {
                hosts[k] = v;
            }
            resolver->setHosts(hosts);
        }

        // 初始化 Tunnel (核心路由控制器)
        auto tunnel = std::make_shared<tunnel::Tunnel>();
        tunnel->setResolver(resolver);
        // 注册内置策略
        tunnel->addProxy(std::make_shared<adapter::DirectAdapter>());
        tunnel->addProxy(std::make_shared<adapter::RejectAdapter>());
        
        // 1. 加载代理节点配置
        for (const auto& proxyConf : cfg.proxies)
        {
            auto typeIt = proxyConf.find("type");
            auto nameIt = proxyConf.find("name");
            auto serverIt = proxyConf.find("server");
            auto portIt = proxyConf.find("port");
            
            if (typeIt == proxyConf.end() || nameIt == proxyConf.end() || 
                serverIt == proxyConf.end() || portIt == proxyConf.end())
            {
                LOG_WARN("Invalid proxy config: missing required fields");
                continue;
            }

            std::string type = typeIt->second;
            std::string name = nameIt->second;
            std::string server = serverIt->second;
            int port = std::stoi(portIt->second);

            if (type == "socks5")
            {
                adapter::Socks5Adapter::Option opt;
                opt.name = name;
                opt.server = server;
                opt.port = port;
                
                if (proxyConf.count("username")) opt.username = proxyConf.at("username");
                if (proxyConf.count("password")) opt.password = proxyConf.at("password");

                tunnel->addProxy(std::make_shared<adapter::Socks5Adapter>(opt));
                LOG_INFO("Loaded proxy: %s (socks5)", name.c_str());
            }
            else if (type == "ss")
            {
                adapter::ShadowsocksAdapter::Option opt;
                opt.name = name;
                opt.server = server;
                opt.port = port;
                
                if (proxyConf.count("password")) opt.password = proxyConf.at("password");
                if (proxyConf.count("cipher")) opt.cipher = proxyConf.at("cipher");
                if (proxyConf.count("udp")) opt.udp = (proxyConf.at("udp") == "true");

                tunnel->addProxy(std::make_shared<adapter::ShadowsocksAdapter>(opt));
                LOG_INFO("Loaded proxy: %s (shadowsocks)", name.c_str());
            }
            else if (type == "ssr")
            {
                adapter::ShadowsocksRAdapter::Option opt;
                opt.name = name;
                opt.server = server;
                opt.port = port;
                
                if (proxyConf.count("password")) opt.password = proxyConf.at("password");
                if (proxyConf.count("cipher")) opt.cipher = proxyConf.at("cipher");
                if (proxyConf.count("protocol")) opt.protocol = proxyConf.at("protocol");
                if (proxyConf.count("protocol-param")) opt.protocol_param = proxyConf.at("protocol-param");
                if (proxyConf.count("obfs")) opt.obfs = proxyConf.at("obfs");
                if (proxyConf.count("obfs-param")) opt.obfs_param = proxyConf.at("obfs-param");
                if (proxyConf.count("udp")) opt.udp = (proxyConf.at("udp") == "true");

                tunnel->addProxy(std::make_shared<adapter::ShadowsocksRAdapter>(opt));
                LOG_INFO("Loaded proxy: %s (shadowsocksr)", name.c_str());
            }
            else if (type == "vmess")
            {
                adapter::VmessAdapter::Option opt;
                opt.name = name;
                opt.server = server;
                opt.port = port;
                
                if (proxyConf.count("uuid")) opt.uuid = proxyConf.at("uuid");
                if (proxyConf.count("alterId")) opt.alterId = std::stoi(proxyConf.at("alterId"));
                if (proxyConf.count("cipher")) opt.cipher = proxyConf.at("cipher");
                if (proxyConf.count("udp")) opt.udp = (proxyConf.at("udp") == "true");
                if (proxyConf.count("tls")) opt.tls = (proxyConf.at("tls") == "true");
                if (proxyConf.count("network")) opt.network = proxyConf.at("network");
                if (proxyConf.count("ws-path")) opt.wsPath = proxyConf.at("ws-path");
                if (proxyConf.count("ws-headers")) opt.wsHeaders = proxyConf.at("ws-headers");

                tunnel->addProxy(std::make_shared<adapter::VmessAdapter>(opt));
                LOG_INFO("Loaded proxy: %s (vmess)", name.c_str());
            }
            else
            {
                LOG_WARN("Unsupported proxy type: %s", type.c_str());
            }
        }

        // 2. 加载代理组 (第一遍：创建对象)
        // 代理组可能引用其他代理或代理组，因此需要分两步加载
        for (const auto& groupConf : cfg.proxyGroups)
        {
            if (groupConf.type == "select")
            {
                tunnel->addProxy(std::make_shared<adapter::SelectorAdapter>(groupConf.name));
                LOG_INFO("Created group: %s (select)", groupConf.name.c_str());
            }
            else if (groupConf.type == "url-test")
            {
                auto urlTest = std::make_shared<adapter::URLTestAdapter>(
                    groupConf.name, groupConf.url, groupConf.interval, io_context);
                tunnel->addProxy(urlTest);
                // 立即启动延迟测试
                urlTest->start();
                LOG_INFO("Created group: %s (url-test)", groupConf.name.c_str());
            }
            else if (groupConf.type == "fallback")
            {
                auto fallback = std::make_shared<adapter::FallbackAdapter>(
                    groupConf.name, groupConf.url, groupConf.interval, io_context);
                tunnel->addProxy(fallback);
                fallback->start();
                LOG_INFO("Created group: %s (fallback)", groupConf.name.c_str());
            }
            else if (groupConf.type == "load-balance")
            {
                adapter::LoadBalanceAdapter::Strategy strategy = adapter::LoadBalanceAdapter::Strategy::ConsistentHashing;
                if (groupConf.strategy == "round-robin")
                {
                    strategy = adapter::LoadBalanceAdapter::Strategy::RoundRobin;
                }
                auto lb = std::make_shared<adapter::LoadBalanceAdapter>(groupConf.name, strategy);
                tunnel->addProxy(lb);
                LOG_INFO("Created group: %s (load-balance)", groupConf.name.c_str());
            }
            else
            {
                LOG_WARN("Unsupported group type: %s", groupConf.type.c_str());
            }
        }

        // 3. 加载代理组 (第二遍：链接代理)
        // 将具体的代理对象注入到代理组中
        for (const auto& groupConf : cfg.proxyGroups)
        {
            auto p = tunnel->proxy(groupConf.name);
            if (!p) continue;

            std::vector<std::shared_ptr<adapter::ProxyAdapter>> groupProxies;
            for (const auto& proxyName : groupConf.proxies)
            {
                auto subProxy = tunnel->proxy(proxyName);
                if (subProxy)
                {
                    groupProxies.push_back(subProxy);
                }
                else
                {
                    LOG_WARN("Proxy group %s references missing proxy: %s", groupConf.name.c_str(), proxyName.c_str());
                }
            }

            if (groupConf.type == "select")
            {
                auto selector = std::dynamic_pointer_cast<adapter::SelectorAdapter>(p);
                if (selector) selector->setProxies(groupProxies);
            }
            else if (groupConf.type == "url-test")
            {
                auto urlTest = std::dynamic_pointer_cast<adapter::URLTestAdapter>(p);
                if (urlTest) urlTest->setProxies(groupProxies);
            }
            else if (groupConf.type == "fallback")
            {
                auto fallback = std::dynamic_pointer_cast<adapter::FallbackAdapter>(p);
                if (fallback) fallback->setProxies(groupProxies);
            }
            else if (groupConf.type == "load-balance")
            {
                auto lb = std::dynamic_pointer_cast<adapter::LoadBalanceAdapter>(p);
                if (lb) lb->setProxies(groupProxies);
            }
        }

        // 应用路由规则和运行模式
        tunnel->setRules(cfg.rules);
        tunnel->setMode(cfg.general.mode);

        // 启动监听器 (Listeners)
        std::vector<std::shared_ptr<listener::Listener>> listeners;

        // 启动 HTTP 代理监听
        if (cfg.general.port > 0)
        {
            auto httpListener = std::make_shared<listener::TcpListener>(io_context, cfg.general.port, tunnel);
            httpListener->start();
            listeners.push_back(httpListener);
        }

        // 启动 SOCKS5 代理监听
        if (cfg.general.socksPort > 0)
        {
            auto socksListener = std::make_shared<listener::TcpListener>(io_context, cfg.general.socksPort, tunnel);
            socksListener->start();
            listeners.push_back(socksListener);
        }

        // 启动外部控制器 (External Controller)
        std::shared_ptr<control::HttpController> controller;
        if (!cfg.general.externalController.empty())
        {
            std::string addr = "127.0.0.1";
            int port = 9090;
            
            size_t colon = cfg.general.externalController.find(':');
            if (colon != std::string::npos)
            {
                addr = cfg.general.externalController.substr(0, colon);
                if (addr.empty())
                {
                    addr = "0.0.0.0";
                }
                try
                {
                    port = std::stoi(cfg.general.externalController.substr(colon + 1));
                }
                catch (...) {}
            }
            else
            {
                // 如果没有冒号，检查是否纯数字（端口）
                bool isPort = !cfg.general.externalController.empty() && 
                              std::all_of(cfg.general.externalController.begin(), cfg.general.externalController.end(), ::isdigit);
                if (isPort)
                {
                    try
                    {
                        port = std::stoi(cfg.general.externalController);
                    }
                    catch (...) {}
                }
                else
                {
                    addr = cfg.general.externalController;
                }
            }
            
            controller = std::make_shared<control::HttpController>(io_context, addr, port, tunnel, cfg.general.externalUI);
            controller->start();
        }

        // 启动事件循环 (Event Loop)
        // 这是一个阻塞调用，直到所有异步任务完成或被停止
        LOG_INFO("Starting Event Loop...");
        io_context.run();
        
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Fatal error: %s", e.what());
        return 1;
    }

    return 0;
}
