#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/reflection/EnumName.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

TEST_CASE("简单反射实例") {
    enum MyEnum {
        Z = -100,
        A = 0,
        B = 10,
        C = 100
    };
    CHECK(reflection::toEnumName(static_cast<MyEnum>(-100)) == "Z");
    CHECK(reflection::toEnumName(static_cast<MyEnum>(0))    == "A");
    CHECK(reflection::toEnumName(static_cast<MyEnum>(10))   == "B");
    CHECK(reflection::toEnumName(static_cast<MyEnum>(100))  == "C");

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