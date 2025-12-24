# Porting TODO List

## Dependencies
Identify and install C++ equivalents for Go libraries:
- Networking: `boost::asio` or `standalone asio`
- YAML Parsing: `yaml-cpp`
- Logging: `spdlog` or `glog`
- JSON: `nlohmann/json`
- HTTP: `cpp-httplib` or `boost::beast`

## Modules to Port
- [ ] `config`: Parse config.yaml
- [ ] `log`: Setup logging system
- [ ] `constant`: Define constants
- [ ] `common`: Port utility functions
- [ ] `component`: Port core components
- [ ] `dns`: Implement DNS resolver
- [ ] `listener`: Implement TCP/UDP listeners
- [ ] `tunnel`: Implement traffic tunnel
- [ ] `rule`: Implement routing rules
- [ ] `adapter`: Implement proxy adapters (Shadowsocks, VMess, etc.)
- [ ] `hub`: Implement REST API

## Next Steps
1. Start with `config` and `log` to get the basic application running.
2. Implement `listener` to accept connections.
3. Implement `tunnel` to handle traffic flow.
