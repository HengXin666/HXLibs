#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <iostream>
#include <HXSTL/utils/ToString.hpp>

TEST_CASE("测试 toSring -> std::pair") {
    std::cout << "输出: ";
    constexpr std::pair<int, std::pair<int, double>> pr{1, {2, 3.14}};
    auto res = HX::STL::utils::toString(pr);
    std::cout << res << '\n';
    CHECK(res == "(1,(2,3.14))");
}

TEST_CASE("测试 toSring -> char []") {
    std::cout << "输出: ";
    char str[] = "char []";
    auto res = HX::STL::utils::toString(str);
    std::cout << res << '\n';
    CHECK(res == std::string {"char []", 7});
    CHECK("\"" + res + "\"" == HX::STL::utils::toString(res));
}

TEST_CASE("测试 toString -> std::tuple") {
    std::cout << "输出: ";
    std::tuple<int, double, std::string> t{1, 3.14, "0x3f"};
    auto res = HX::STL::utils::toString(t);
    std::cout << res << '\n';
    CHECK(res == R"((1,3.14,"0x3f"))");
}