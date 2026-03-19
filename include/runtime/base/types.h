#pragma once


/**
 * 统一类型风格
 */
#include <cstdint>
#include <string>
namespace runtime::base{

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t; 

using String = std::string;

} // namespace runtime::base;