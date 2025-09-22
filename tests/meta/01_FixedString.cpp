#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/meta/FixedString.hpp>

TEST_CASE("FixedString") {
    constexpr std::string_view str{"123"};
    using namespace HX;
    constexpr meta::FixedString<str.size() + 1> fStr{str.data()};
    static_assert(str.size() == 3);
    constexpr meta::FixedString arrfStr{"123"};

    static_assert(meta::ToCharPack<fStr>::view() == str, "");
    static_assert(meta::ToCharPack<arrfStr>::view() == str, "");
}