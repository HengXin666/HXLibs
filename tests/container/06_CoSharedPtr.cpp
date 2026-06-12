#include <HXLibs/container/CoSharedPtr.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace container;

TEST_CASE("基本使用") {
    auto p = makeCoSharedPtr<bool>(true);
    auto nullPtr = decltype(p){};
    auto nullPtr2 = decltype(p){nullptr};
    CHECK(p.useCount() == 1);
    CHECK(nullPtr.useCount() == 0);
    CHECK(nullPtr2.useCount() == 0);
    CHECK(nullPtr == nullPtr2);

    auto p2 = p;
    CHECK(p2 == p);
    CHECK((p2.useCount() == p.useCount() && p2.useCount() == 2));
    p2.swap(p);
    CHECK((p2.useCount() == p.useCount() && p2.useCount() == 2));
    {
        auto p3 = nullPtr;
        p3 = p2; // 拷贝赋值
        CHECK((p2.useCount() == p3.useCount() && p3.useCount() == 3));
    }
    CHECK((p2.useCount() == p.useCount() && p2.useCount() == 2)); // 测试析构
    p.reset(false);
    CHECK((p2.useCount() == p.useCount() && p2.useCount() == 1)); // 测试析构
    
    p = std::move(p2);
    CHECK((p.useCount() == 1)); // 测试析构
}