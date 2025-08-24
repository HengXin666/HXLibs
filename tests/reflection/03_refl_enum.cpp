#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/reflection/EnumName.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

TEST_CASE("简单反射实例") {
    enum MyEnum : int {
        Z = -100,
        A = 0,
        B = 10,
        C = 100
    };
    CHECK(reflection::toEnumName(static_cast<MyEnum>(-100)) == "Z");
    CHECK(reflection::toEnumName(static_cast<MyEnum>(0))    == "A");
    CHECK(reflection::toEnumName(static_cast<MyEnum>(10))   == "B");
    CHECK(reflection::toEnumName(static_cast<MyEnum>(100))  == "C");

    static_assert(reflection::isValidEnumValue<MyEnum, (MyEnum)0> == true, "");
    static_assert(reflection::isValidEnumValue<MyEnum, (MyEnum)1> == false, "");
    static_assert(reflection::isValidEnumValue<MyEnum, (MyEnum)1433223> == false, "");

    // 测试 哈希
    constexpr auto res = reflection::internal::EnumValueMapStr<MyEnum>{}();
    static_assert(res.at(static_cast<MyEnum>(0)) == "A", "");
    static_assert(res.find(static_cast<MyEnum>(1)) == res.end(), "");

    // 支持获取枚举个数
    static_assert(reflection::getValidEnumValueCnt<MyEnum>() == 4, "");

    static_assert(reflection::toEnumName(static_cast<MyEnum>(-100)) == "Z", "");
    static_assert(reflection::toEnumName(static_cast<MyEnum>(0))    == "A", "");
    static_assert(reflection::toEnumName(static_cast<MyEnum>(10))   == "B", "");
    static_assert(reflection::toEnumName(static_cast<MyEnum>(100))  == "C", "");

    CHECK(reflection::toEnum<MyEnum>("Z") == -100);

    enum class MyEnumClass : char {
        Z = -100,
        A = 0,
        B = 10,
        C = 100
    };
    CHECK(reflection::toEnumName(static_cast<MyEnumClass>(-100)) == "Z");
    CHECK(reflection::toEnumName(static_cast<MyEnumClass>(0))    == "A");
    CHECK(reflection::toEnumName(static_cast<MyEnumClass>(10))   == "B");
    CHECK(reflection::toEnumName(static_cast<MyEnumClass>(100))  == "C");

    CHECK(reflection::toEnum<MyEnumClass>("Z") == MyEnumClass::Z);

    enum class MyUEnumClass : uint8_t {
        A, B, C, D, E, F, G
    };
    CHECK(reflection::toEnumName(static_cast<MyUEnumClass>(0)) == "A");
    CHECK(reflection::toEnum<MyUEnumClass>("B") == MyUEnumClass::B);
}