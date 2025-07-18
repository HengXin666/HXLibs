#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/log/Log.hpp>

#include <string_view>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/container/CHashMap.hpp>

using namespace HX;

TEST_CASE("test") {
    constexpr container::CHashMap<std::string_view, std::size_t, 4> mp{
        std::array<std::pair<std::string_view, std::size_t>, 4>{
            std::pair<std::string_view, std::size_t>{"1", 1},
            {"2", 1},
            {"3", 1},
            {"4", 1},
        }
    };

    static_assert(mp.at("1") == 1, "111");
    static_assert(mp.at("2") == 1, "111");
}