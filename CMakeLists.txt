cmake_minimum_required(VERSION 3.14)
project(clash-core-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

include(FetchContent)

# download yaml-cpp using mirror
# original yaml-cpp: https://github.com/jbeder/yaml-cpp.git
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://bgithub.xyz/jbeder/yaml-cpp.git
    GIT_TAG master
)

# original asio: https://github.com/chriskohlhoff/asio.git
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://bgithub.xyz/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)

# original nlohmann/json: https://github.com/nlohmann/json.git
FetchContent_Declare(
    json
    GIT_REPOSITORY https://bgithub.xyz/nlohmann/json.git
    GIT_TAG v3.11.2
)

# original libmaxminddb: https://github.com/maxmind/libmaxminddb.git
FetchContent_Declare(
    libmaxminddb
    GIT_REPOSITORY https://bgithub.xyz/maxmind/libmaxminddb.git
    GIT_TAG 1.7.1
    GIT_SUBMODULES "" # Disable submodules
)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp asio json libmaxminddb)

include_directories(include)
include_directories(${asio_SOURCE_DIR}/asio/include)
include_directories(${json_SOURCE_DIR}/include)
include_directories(${libmaxminddb_SOURCE_DIR}/include)

add_definitions(-DASIO_STANDALONE)

# 获取当前构建时间并定义为宏
string(TIMESTAMP CURRENT_BUILD_TIME "%Y-%m-%d %H:%M:%S")
add_definitions(-DBUILD_TIME=\"${CURRENT_BUILD_TIME}\")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

file(GLOB_RECURSE SOURCES "src/*.cpp")

if(WIN32)
    list(FILTER SOURCES EXCLUDE REGEX "platform_linux.cpp$")
    add_definitions(-D_WIN32_WINNT=0x0601 -DNOMINMAX)
else()
    list(FILTER SOURCES EXCLUDE REGEX "platform_windows.cpp$")
endif()

add_executable(clash-core-cpp ${SOURCES})

if(WIN32)
    target_link_libraries(clash-core-cpp yaml-cpp OpenSSL::SSL OpenSSL::Crypto maxminddb Ws2_32 Iphlpapi Mswsock Crypt32)
else()
    target_link_libraries(clash-core-cpp yaml-cpp pthread OpenSSL::SSL OpenSSL::Crypto maxminddb)
endif()

