cmake_minimum_required(VERSION 3.14)
project(clash-core-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

include(FetchContent)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG master
)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

FetchContent_Declare(
    libmaxminddb
    GIT_REPOSITORY https://github.com/maxmind/libmaxminddb.git
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

add_executable(clash-core-cpp ${SOURCES})

target_link_libraries(clash-core-cpp yaml-cpp pthread OpenSSL::SSL OpenSSL::Crypto maxminddb)

