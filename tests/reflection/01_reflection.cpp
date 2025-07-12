#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/reflection/MemberName.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;


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