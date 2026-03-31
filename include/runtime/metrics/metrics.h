#pragma once

#include "runtime/base/noncopyable.h"

#include <string>

namespace runtime::metric {

class Metric : public runtime::base::NonCopyable{
public:
    virtual ~Metric() = default;
    virtual std::string ToString() const = 0;
};
}
