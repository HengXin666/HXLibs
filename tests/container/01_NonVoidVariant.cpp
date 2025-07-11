#include <HXLibs/container/UninitializedNonVoidVariant.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX::container;

struct A {
    // std::variant 不允许...
    // A() = delete;

    constexpr bool operator==(A const&) const noexcept {
        return true;
    }
};

TEST_CASE("NonVoidVariant") {
    UninitializedNonVoidVariant<A, int, int, void> v, vv;
    // emplace
    CHECK(v.index() == UninitializedNonVoidVariantNpos);
    v.emplace<int>(1);
    CHECK(v.index() == 1);
    v.emplace<2>(1);
    CHECK(v.index() == 2);

    // operator==
    {
        vv.emplace<1>(2);
        bool res = v == vv;
        CHECK(res == false);
    }
    // reset
    {
        v.reset();
        vv.reset();
        bool res = v == vv;
        CHECK(res == true);
    }
    {
        v.emplace<void>();
        vv.emplace<void>();
        bool res = v == vv;
        CHECK(res == true);
    }
    {
        v.emplace<int>(2233);
        vv.emplace<int>(2233);
        bool res = v == vv;
        CHECK(res == true);
    }

    // swap
    v.swap(vv);
    {
        bool res = v == vv;
        CHECK(res == true);
    }

    // get
    {
        v.reset();
        v.swap(vv);
        bool res = v == vv;
        CHECK(res == false);
        CHECK(get<int>(v) == 2233);
        try {
            get<int>(vv); // vv = null, get 会抛异常
            CHECK(false);
        } catch (...) {
            CHECK(true);
        }
    }
}

#include <variant>

TEST_CASE("std::variant") {
    std::variant<A, int, int, NonVoidType<void>> v{};
    // CHECK(v.index() == std::variant_npos); // v.index() = 0
    // v.emplace<int>(1); // 调用已删除的成员函数 'emplace'clang(ovl_deleted_call)
    // CHECK(v.index() == 0);
    v.emplace<2>(1);
    CHECK(v.index() == 2);
}