#pragma once

#include "constant/metadata.h"
#include <string>
#include <chrono>

namespace clash {
namespace common {

class Trackable {
public:
    virtual ~Trackable() = default;
    
    virtual std::string id() const = 0;
    virtual constant::Metadata metadata() const = 0;
    virtual uint64_t upload() const = 0;
    virtual uint64_t download() const = 0;
    virtual std::chrono::system_clock::time_point startTime() const = 0;
    virtual std::string chain() const = 0;
    virtual void close() = 0;
};

} // namespace common
} // namespace clash
