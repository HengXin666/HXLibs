#pragma once

#include <string_view>

namespace benchmark_payloads {

inline constexpr std::string_view hello = "Hello World!";

inline constexpr std::string_view json = R"json({"users":[{"id":1001,"name":"Alice","active":true,"roles":["admin","editor"]},{"id":1002,"name":"Bob","active":true,"roles":["viewer"]},{"id":1003,"name":"Carol","active":false,"roles":["viewer","billing"]}],"page":1,"page_size":20,"total":3})json";

} // namespace benchmark_payloads
