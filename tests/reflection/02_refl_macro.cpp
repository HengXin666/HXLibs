#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/reflection/ReflectionMacro.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

template <auto ptr>
inline constexpr std::string_view getMemberName() {
    constexpr std::string_view funcName = __FUNCSIG__;
    return funcName;
}

TEST_CASE("对照组: 无宏反射") {
    struct Case01 {
        int a;
        std::string b;
        std::vector<Case01> c;
    };

    struct Case02 {
        int num;
        std::string str;
        std::vector<Case02> arr;
    };

    constexpr auto N = reflection::membersCountVal<Case01>;
    constexpr auto name = reflection::getMembersNames<Case01>();

    CHECK(N == 3);
    CHECK(name[0] == "a");
    CHECK(name[1] == "b");
    CHECK(name[2] == "c");

    constexpr auto N2 = reflection::membersCountVal<Case02>;
    constexpr auto name2 = reflection::getMembersNames<Case02>();

    CHECK(N2 == 3);
    CHECK(name2[0] == "num");
    CHECK(name2[1] == "str");
    CHECK(name2[2] == "arr");

    {
        constexpr auto tp1 = reflection::internal::getStaticObjPtrTuple<Case01>();
        constexpr auto tp2 = reflection::internal::getStaticObjPtrTuple<Case02>();
        std::cout << getMemberName<get<0>(tp1)>() << '\n';
        std::cout << getMemberName<get<0>(tp2)>() << '\n';
    }
#if 0
    {
        log::hxLog.debug(reflection::internal::getStaticObjPtrTuple<Case01>());
        log::hxLog.debug(reflection::internal::getStaticObjPtrTuple<Case02>());
    }
    {
        constexpr auto tp1 = reflection::internal::getStaticObjPtrTuple<Case01>();
        constexpr auto tp2 = reflection::internal::getStaticObjPtrTuple<Case02>();

        CHECK(get<0>(tp1) == get<0>(tp2));

        log::hxLog.debug(getMemberName<get<0>(tp1)>());
        log::hxLog.debug(getMemberName<get<0>(tp2)>());
    }
#endif
}


#if 0
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
    constexpr auto N = reflection::membersCountVal<TestCase01>;
    constexpr auto name = reflection::getMembersNames<TestCase01>();

    CHECK(N == 3);
    CHECK(name[0] == "num");
    CHECK(name[1] == "str");
    CHECK(name[2] == "arr");

    TestCase01 t;
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