#include <HXLibs/reflection/MemberPtr.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

TEST_CASE("测试类成员指针到字段名称的映射") {
    struct A {
        int a, b, c;
    };
    using namespace HX::reflection;
    auto t = makeMemberPtrToNameMap<A>();
    auto res = t.at(&A::a); // 非编译期, 但可作为运行时单例 (static 局部变量)
    CHECK(res == "a");
}