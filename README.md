# Clash Core C++

A C++17 implementation of the [Clash](https://github.com/Dreamacro/clash) core, designed to be lightweight and efficient.

## 🚧 Project Status

**Current Phase: Initialization & Scaffolding**

The project structure has been established, mirroring the original Go implementation. Basic class interfaces have been defined for core modules.

- **Adapter Module**: Core proxy adapters (`Shadowsocks`, `VMess`, `Socks5`, `URLTest`, etc.) interfaces are defined.
  - Code style: **Allman Style** (Braces on new lines).
  - Documentation: Detailed Chinese comments added to headers.
- **Infrastructure**: CMake build system is ready.

## 🛠 Build Instructions

### Prerequisites
- **C++ Compiler**: Requires C++17 support (GCC 8+, Clang 7+, MSVC 2019+).
- **CMake**: Version 3.10 or higher.

### Build Steps
```bash
mkdir build
cd build
cmake ..
make
```

## 📂 Project Structure

```text
clash-core-cpp/
├── include/          # Header files
│   ├── adapter/      # Proxy adapter interfaces (Allman style)
│   ├── common/       # Utilities (Crypto, Net, etc.)
│   ├── config/       # Configuration parsing
│   ├── constant/     # Global constants
│   ├── dns/          # DNS resolver
│   ├── listener/     # Inbound listeners (TCP/UDP)
│   ├── log/          # Logging interface
│   ├── rule/         # Routing rules
│   └── tunnel/       # Traffic tunnel logic
├── src/              # Source implementations
├── CMakeLists.txt    # Build configuration
└── README.md
```

## 📝 Development Guidelines


### Dependencies (Planned)
- **Networking**: `asio` (Standalone or Boost)
- **Configuration**: `yaml-cpp`
- **Logging**: `spdlog`
- **JSON**: `nlohmann/json`
- **Cryptography**: `OpenSSL` 



