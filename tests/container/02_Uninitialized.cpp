#include <HXLibs/container/Uninitialized.hpp>

#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX::container;

// 测试类型：构造/析构记录
struct TestType {
    static inline int ctor_count = 0;
    static inline int dtor_count = 0;

    std::string value;

    TestType(std::string v) : value(std::move(v)) {
        ++ctor_count;
    }

    ~TestType() {
        ++dtor_count;
    }
};

TEST_CASE("Uninitialized") {
    Uninitialized<int> u1;
    u1.set(1);
    CHECK(u1.move() == 1);

    struct A {
        ~A() noexcept {
            HX::log::hxLog.info("正确析构");
            CHECK(true);
        }
    };

    Uninitialized<A> u2;
    u2.set(A{});
}

TEST_CASE("Uninitialized RAII") {
    {
        Uninitialized<TestType> holder;
        holder.set("hello");
    }

    // 构造 1 次, 析构 1 次
    CHECK(TestType::ctor_count == 1);
    CHECK(TestType::dtor_count == 1);
}