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

    struct TestErr {
        std::thread _1; // 不是聚和类
    };

    CHECK(reflection::membersCountVal<TestErr> != 1);
}