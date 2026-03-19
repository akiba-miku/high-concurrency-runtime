#pragma once

#include <string>

namespace runtime::base {

std::string ErrorToString(int err);
std::string GetLastErrorString();

} // namespace runtime::base;