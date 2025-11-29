#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/meta/FixedString.hpp>

using namespace HX;

TEST_CASE("FixedString") {
    constexpr std::string_view str{"123"};
    constexpr meta::FixedString<str.size() + 1> fStr{str.data()};
    static_assert(str.size() == 3);
    constexpr meta::FixedString arrfStr{"123"};

    static_assert(meta::ToCharPack<fStr>::view() == str, "");
    static_assert(meta::ToCharPack<arrfStr>::view() == str, "");
}

TEST_CASE("FixedString op") {
    static_assert(meta::ToCharPack<meta::FixedString{"a"} | "a">::view() == "aa");
    static_assert(meta::ToCharPack<meta::FixedString{"a"} | "a" | "a">::view() == "aaa");
    static_assert(meta::ToCharPack<"a" | meta::FixedString{"a"} | "a">::view() == "aaa");
}