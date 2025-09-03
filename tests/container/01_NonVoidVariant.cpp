#include <HXLibs/container/UninitializedNonVoidVariant.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace container;

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
        CHECK(get<int, exception::ExceptionMode::Nothrow>(v) == 2233);
        try {
            get<int>(vv); // vv = null, get 会抛异常
            CHECK(false);
        } catch (...) {
            CHECK(true);
        }
    }
}

TEST_CASE("测试自定义打印") {
    struct NoMove {
        NoMove& operator=(NoMove&&) = delete;
    };

    UninitializedNonVoidVariant<int, std::string, NoMove> v{"123"};
    log::hxLog.info("v:", v);
    v = 123;
    log::hxLog.info("v:", v);
    struct sb {
        int x = 666;

        auto func(UninitializedNonVoidVariant<int, std::string> const& v) {
            return container::visit([&](auto&&) {
                return;
            }, v);
        }

        void func(int);
    };
}

#include <variant>

TEST_CASE("std::variant") {
    std::variant<A, int, int, NonVoidType<>> v{};
    // CHECK(v.index() == std::variant_npos); // v.index() = 0
    // v.emplace<int>(1); // 调用已删除的成员函数 'emplace'clang(ovl_deleted_call)
    // CHECK(v.index() == 0);
    v.emplace<2>(1);
    CHECK(v.index() == 2);
}

TEST_CASE("构造 U to T") {
    UninitializedNonVoidVariant<std::string, double, int> u{"123"};
    CHECK(u.get<0>() == "123");
    u = "456";
    CHECK(u.get<0>() == "456");
    u = 789;
    CHECK(u.get<int>() == 789);
    u = 789.101112;
    CHECK(u.get<double>() == 789.101112);

    struct Base {
        int a;
    };

    struct Sub : public Base {
        Sub(int _1, int _2)
            : Base(_1)
            , v{_2}
        {}

        int v;
    };

    UninitializedNonVoidVariant<Base, Sub> uu{};
    uu.emplace<Base>(0);
    uu.emplace<Sub>(1, 1);

    uu = Base{0};
    // uu = Sub{1, 1}; // 没人会这样写

#if 0
    // ub: 不能使用引用作为参数!
    Base base{1};
    Sub sub{3, 4};
    UninitializedNonVoidVariant<Base&, int> uuu;
    uuu = base;
    log::hxLog.info("uuu<Base>:", uuu.get<Base&>());
    uuu = sub;
    log::hxLog.info("uuu<sub>:", uuu.get<Base&>());
#endif
    struct sb {
        int x = 666;
        
        auto func(std::variant<int, std::string> const& v) {
            return std::visit([&](auto&&) -> std::string {
                return log::formatString(x);
            }, v);
        }
    };
}