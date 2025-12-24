# Clash Core C++ Port

This is a C++ port of the [clash-core](https://github.com/Dreamacro/clash) project.

## Project Structure

The project structure mirrors the original Go project:

- `src/`: Source code
    - `adapter/`: Proxy adapters
    - `common/`: Common utilities
    - `component/`: Core components
    - `config/`: Configuration handling
    - `constant/`: Constants
    - `context/`: Context management
    - `dns/`: DNS server/client
    - `hub/`: Hub (REST API, etc.)
    - `listener/`: Inbound listeners
    - `log/`: Logging
    - `rule/`: Routing rules
    - `transport/`: Transport layer
    - `tunnel/`: Tunnel management
- `include/`: Header files

## Building

Prerequisites:
- CMake
- C++ Compiler (GCC/Clang/MSVC) supporting C++17

```bash
mkdir build
cd build
cmake ..
make
./clash-core-cpp
```
