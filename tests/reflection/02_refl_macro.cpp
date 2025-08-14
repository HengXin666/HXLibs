#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/ReflectionMacro.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

#if 1
struct TestCase01 {
private:
    int a;
    std::string b;
    std::vector<TestCase01> c;
public:
    HX_REFL(a, b, c)
};

TEST_CASE("测试宏反射私有变量") {
    constexpr auto N = reflection::membersCountVal<TestCase01>;
    constexpr auto name = reflection::getMembersNames<TestCase01>();

    CHECK(N == 3);
    CHECK(name[0] == "a");
    CHECK(name[1] == "b");
    CHECK(name[2] == "c");

    TestCase01 t;
    reflection::forEach(t, [] <std::size_t Idx> (std::index_sequence<Idx>, auto name, auto& v) {
        if constexpr (Idx == 0) {
            CHECK(name == "a");
            v = 1433223;
        } else if constexpr (Idx == 1) {
            CHECK(name == "b");
            v = "str";
        } else if constexpr (Idx == 2) {
            CHECK(name == "c");
            v = {};
        } else {
            CHECK(false);
        }
    });
}

struct TestCase02 {
private:
    int a;
    std::string b;
    std::vector<TestCase01> c;
public:
    HX_REFL_AS(
        a, num, 
        b, str,
        c, arr
    )
};

TEST_CASE("测试宏反射私有变量") {
    constexpr auto N = reflection::membersCountVal<TestCase02>;
    constexpr auto name = reflection::getMembersNames<TestCase02>();

    CHECK(N == 3);
    CHECK(name[0] == "num");
    CHECK(name[1] == "str");
    CHECK(name[2] == "arr");

    TestCase02 t;
    reflection::forEach(t, [] <std::size_t Idx> (std::index_sequence<Idx>, auto name, auto& v) {
        if constexpr (Idx == 0) {
            CHECK(name == "num");
            v = 1433223;
        } else if constexpr (Idx == 1) {
            CHECK(name == "str");
            v = "str";
        } else if constexpr (Idx == 2) {
            CHECK(name == "arr");
            v = {};
        } else {
            CHECK(false);
        }
    });
}
#endif