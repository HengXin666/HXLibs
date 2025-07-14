#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;

#include <unordered_map>

TEST_CASE("initializer_list") {
    struct Test {
        std::initializer_list<int>  name01;
        std::initializer_list<Test> name02;
    };
    Test test{
        {1, 2, 3, 4, 5},
        {{
            {11, 22, 33, 44, 55},
            {}
        }}
    };
    constexpr auto Cnt = reflection::membersCount<Test>();
    CHECK(Cnt == 2);

    constexpr auto Name = reflection::getMembersNames<Test>();
    CHECK(Name[0] == "name01");
    CHECK(Name[1] == "name02");

    log::hxLog.info(test);
}

#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include <any>
#include <tuple>
#include <array>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <stack>
#include <queue>
#include <bitset>
#include <memory>
#include <chrono>
#include <complex>

#include <thread>

#if 0
#include <filesystem>
#include <span>
#include <regex>
#include <future>
#endif

TEST_CASE("cnt") {
    struct Test {
        std::string _1;
        std::string_view _2;
        std::optional<int> _3;
        std::variant<int, float, std::string> _4;
        std::any _5;

        std::unique_ptr<int> _6;
        std::shared_ptr<int> _7;
        std::weak_ptr<int> _8;

        std::vector<int> _9;
        std::deque<int> _10;
        std::list<int> _11;
        std::forward_list<int> _12;

        std::array<int, 5> _13;

        std::set<int> _14;
        std::unordered_set<int> _15;
        std::map<int, int> _16;
        std::unordered_map<int, int> _17;

        std::multiset<int> _18;
        std::unordered_multiset<int> _19;
        std::multimap<int, int> _20;
        std::unordered_multimap<int, int> _21;

        std::stack<int> _22;
        std::queue<int> _23;
        std::priority_queue<int> _24;

        std::chrono::system_clock::time_point _25;
        std::chrono::duration<int> _26;

        std::pair<int, int> _27;
        std::tuple<int, double, std::string> _28;

        std::bitset<32> _29;
        std::complex<double> _30;

        int _31;
        double _32;
        bool _33;
    };

    CHECK(reflection::membersCountVal<Test> == 33);

    struct TestErr {    // 是聚合类
        std::thread _1; // 不是聚和类
    };

    CHECK(reflection::membersCountVal<TestErr> != 1);

    struct HttpHeader {
        std::string_view name;
        std::string_view value;
    };

    CHECK(reflection::membersCountVal<HttpHeader> == 2);

    struct TestOpt {
        std::optional<int> _;
    };

#if 0
    // 转换是模棱两可的
    // Conversion from 'reflection::internal::Any' to 'std::optional<int>' 
    // is ambiguousclang(typecheck_ambiguous_condition)
    if constexpr (requires {
        TestOpt {reflection::internal::Any{}};
    }) {
        CHECK(true);
        log::hxLog.debug("[ok]: TestOpt {reflection::internal::Any{}}");
    } else {
        CHECK(false);
    }
#else
    if constexpr (requires {
        TestOpt {{reflection::internal::Any{}}};
    }) {
        CHECK(true);
        log::hxLog.debug("[ok]: TestOpt {{reflection::internal::Any{}}}");
    } else {
        CHECK(false);
    }
#endif
}

struct Any {
    template <typename T>
    operator T() { return {}; }
};

struct A {
    // 任意参数构造
    template <typename U>
    A(U&&) {
        log::hxLog.warning("type is", __PRETTY_FUNCTION__);
    }
};

// 宏模板妹莉时刻
#define LJ_DEFINE_2(x, y) x##y
#define LJ_DEFINE(x, y) LJ_DEFINE_2(x, y)

// 定义一个辅助定义宏, 方便帮我 (void) 掉
#define B_INTI(CODE)                        \
B LJ_DEFINE(_hx_, __LINE__) {CODE};         \
static_cast<void>(LJ_DEFINE(_hx_, __LINE__))

TEST_CASE("demo") {
    struct B {
        A _;
    };

    B_INTI( {Any{}} );
    B_INTI( A{Any{}} ); // 等价

#if 0
    B_INTI( Any{} ); // 触发 Any{} -> A
                     // 发现 A(U&&)
                     // 由于 U 无法确定为准确类型
                     // 因为 T 需要一个类型来推导, 而 U 也需要一个类型来推导
                     // [[推导死锁]] -> 无法确定 (推导失败, 程序非良构, 抛出)
#endif

#if 0
    if constexpr (requires {
        B {reflection::internal::Any{}};
    }) {
        CHECK(true);
        log::hxLog.debug("[ok]: B {reflection::internal::Any{}}");
    } else {
        CHECK(false);
    }
#else
    if constexpr (requires {
        B {{reflection::internal::Any{}}};
    }) {
        CHECK(true);
        log::hxLog.debug("[ok]: B {{reflection::internal::Any{}}}");
    } else {
        CHECK(false);
    }
#endif
}

TEST_CASE("编译期反射名称") {
    struct A {
        std::string _name01;
        std::string _name02;
        int         _name03;
        A*          _name04;
    };

    constexpr auto aName = reflection::getMembersNames<A>();

    for (std::size_t i = 0; i < reflection::membersCountVal<A>; ++i)
        CHECK(aName[i] == "_name0" + std::to_string(i + 1));
}

TEST_CASE("编译期反射-for") {
    struct A {
        std::string _val01;
        std::vector<int> _val02;
    };

    A a{
        "test",
        {1, 2, 3, 4, 5 ,6, 7, 8, 9}
    };

    log::hxLog.debug(a);

    reflection::forEach(a, [] <std::size_t I> (std::index_sequence<I>, 
        auto name, 
        auto& val
    ) {
        if constexpr (I == 0) {
            CHECK(name == "_val01");
            CHECK(val == "test");
            val = "ok";
        } else if constexpr (I == 1) {
            CHECK(name == "_val02");
            CHECK(val == std::vector{1, 2, 3, 4, 5 ,6, 7, 8, 9});
            val = {11, 4, 5, 14};
        }
    });

    log::hxLog.debug(a);
}