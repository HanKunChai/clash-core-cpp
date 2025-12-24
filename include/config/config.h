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

namespace clash {
namespace config {

struct Controller {
    std::string externalController;
    std::string externalUI;
    std::string secret;
};

struct LegacyInbound {
    int port = 0;
    int socksPort = 0;
    int redirPort = 0;
    int tproxyPort = 0;
    int mixedPort = 0;
    bool allowLan = false;
    std::string bindAddress;
};

struct General : public LegacyInbound, public Controller {
    std::vector<std::string> authentication;
    tunnel::Mode mode = tunnel::Mode::Rule;
    log::Level logLevel = log::Level::Info;
    bool ipv6 = false;
    std::string interfaceName;
    int routingMark = 0;
};

struct FallbackFilter {
    bool geoip = false;
    std::string geoipCode;
    std::vector<std::string> ipcidr;
    std::vector<std::string> domain;
};

struct DNS {
    bool enable = false;
    bool ipv6 = false;
    std::vector<std::string> nameServer;
    std::vector<std::string> fallback;
    FallbackFilter fallbackFilter;
    std::string listen;
    constant::DNSMode enhancedMode = constant::DNSMode::DNSNormal;
    std::vector<std::string> defaultNameserver;
    std::string fakeIPRange;
    std::map<std::string, std::string> hosts;
    std::map<std::string, std::string> nameServerPolicy;
    std::vector<std::string> searchDomains;
};

struct Profile {
    bool storeSelected = false;
    bool storeFakeIP = false;
};

struct Experimental {
    bool udpFallbackMatch = false;
};

struct ProxyGroup {
    std::string name;
    std::string type;
    std::vector<std::string> proxies;
    std::string url;
    int interval = 0;
    std::string strategy;
};

struct Config {
    General general;
    DNS dns;
    Experimental experimental;
    std::map<std::string, std::string> hosts;
    Profile profile;
    std::vector<constant::Inbound> inbounds;
    
    // Proxies
    std::vector<std::map<std::string, std::string>> proxies;
    // Proxy Groups
    std::vector<ProxyGroup> proxyGroups;
    
    std::vector<std::shared_ptr<rule::Rule>> rules;

    static Config load(const std::string& path);
};

} // namespace config
} // namespace clash
