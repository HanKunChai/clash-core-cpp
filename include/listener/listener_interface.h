#pragma once

#include <string>

namespace clash {
namespace listener {

class Listener {
public:
    virtual ~Listener() = default;
    virtual void close() = 0;
    virtual std::string address() const = 0;
};

} // namespace listener
} // namespace clash
