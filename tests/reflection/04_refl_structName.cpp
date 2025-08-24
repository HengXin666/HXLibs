#include <HXLibs/reflection/StructName.hpp>
#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX;

TEST_CASE("测试类型反射 [反例]") {
    // 不建议 使用
    static_assert(reflection::getStructName<int>() == "int", "");
    static_assert(reflection::getStructName<bool>() == "bool", "");
    static_assert(reflection::getStructName<float>() == "float", "");
    log::hxLog.info(reflection::getStructName<const char *>());
    static_assert(reflection::getStructName<unsigned int>() == "unsigned int", "");
}

namespace test {

struct Student {
    struct ClassRoom {
        int id;
    };
    std::string name;
};

template <typename T, std::size_t N>
struct Array {
    T arr[N];
};

using hxArray = Array<double, 0721>;

using hxVector = Array<Array<int, 2>, 3>;

}

template <typename... Args>
struct Tmp {};

TEST_CASE("测试类型反射: struct") {
    struct Student {
        struct ClassRoom {
            int id;
        };
        std::string name;
    };
    static_assert(reflection::getStructName<Student>() == "Student", "");
    // 只能获取到 类名, 不包含任何的 :: 嵌套
    static_assert(reflection::getStructName<Student::ClassRoom>() == "ClassRoom", "");
    static_assert(reflection::getStructName<struct Abcd>() == "Abcd", "");

    // 支持命名空间
    static_assert(reflection::getStructName<test::Student>() == "Student", "");
    static_assert(reflection::getStructName<test::Student::ClassRoom>() == "ClassRoom", "");

    // 支持模板
    static_assert(reflection::getStructName<Tmp<int, double, Tmp<Tmp<>>>>() == "Tmp", "");
    static_assert(reflection::getStructName<test::Array<int, 3>>() == "Array", "");
    static_assert(reflection::getStructName<test::Array<struct ONaNi, 114514>>() == "Array", "");

    // 如果是别名, 只能获取到 最初始的 名称
    static_assert(reflection::getStructName<test::hxArray>() == "Array", "");
    static_assert(reflection::getStructName<test::hxVector>() == "Array", "");
}

namespace test2 {

class Student {
public:
    class ClassRoom {
    public:
        int id;
    };
    std::string name;
};

template <typename T, std::size_t N>
class Array {
    T arr[N];
};

using hxArray = Array<double, 0721>;

using hxVector = Array<Array<int, 2>, 3>;

}

template <typename... Args>
class Tmp2 {};

TEST_CASE("测试类型反射: class") {
    // class 理论上同理
    class Student {
    public:
        class ClassRoom {
        public:
            int id;
        };
        std::string name;
    };
    static_assert(reflection::getStructName<Student>() == "Student", "");
    // 只能获取到 类名, 不包含任何的 :: 嵌套
    static_assert(reflection::getStructName<Student::ClassRoom>() == "ClassRoom", "");
    static_assert(reflection::getStructName<class Abcd>() == "Abcd", "");

    // 支持命名空间
    static_assert(reflection::getStructName<test2::Student>() == "Student", "");
    static_assert(reflection::getStructName<test2::Student::ClassRoom>() == "ClassRoom", "");

    // 支持模板
    static_assert(reflection::getStructName<Tmp2<int, double, Tmp2<Tmp2<>>>>() == "Tmp2", "");
    static_assert(reflection::getStructName<test2::Array<int, 3>>() == "Array", "");
    static_assert(reflection::getStructName<test2::Array<struct ONaNi, 114514>>() == "Array", "");

    // 如果是别名, 只能获取到 最初始的 名称
    static_assert(reflection::getStructName<test2::hxArray>() == "Array", "");
    static_assert(reflection::getStructName<test2::hxVector>() == "Array", "");
}

TEST_CASE("测试类型反射: enum") {
    enum Code {};
    enum class CodePrimer {};

    using HttpCode = Code;

    static_assert(reflection::getStructName<Code>() == "Code", "");
    static_assert(reflection::getStructName<CodePrimer>() == "CodePrimer", "");

    // 如果是别名, 只能获取到 最初始的 名称
    static_assert(reflection::getStructName<HttpCode>() == "Code", "");
}

TEST_CASE("测试类型反射: union") {
    union Uu {};

    using MyUu = Uu;

    static_assert(reflection::getStructName<Uu>() == "Uu", "");

    // 如果是别名, 只能获取到 最初始的 名称
    static_assert(reflection::getStructName<MyUu>() == "Uu", "");
}