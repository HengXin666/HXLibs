#include <HXLibs/reflection/ReflectionMacro.hpp>

#define sbstr(sb) strstr(sb)
#define strstr(str) #str

#include <iostream>


using namespace HX;

struct HXTest_1 {
private:
    [[maybe_unused]] int a{};
    [[maybe_unused]] std::string b{};
    [[maybe_unused]] double c{};
public:
    static void cou() {
        std::cout << sbstr((HX_REFL_AS(
            a, sb_a,
            b, fk_b,
            c, sb_c
        ))) << '\n' << '\n';
    }

    HX_REFL_AS(
        a, sb_a,
        b, fk_b, 
        c, sb_c
    )
};

struct HXTest_2 {
private:
    [[maybe_unused]] int a{};
    [[maybe_unused]] std::string b{};
    [[maybe_unused]] double c{};
public:
    static void cou() {
        std::cout << sbstr((HX_REFL_AS(
            a, fk_a, 
            c, fk_c
        ))) << '\n' << '\n';
    }

    HX_REFL_AS(
        a, fk_a, 
        c, fk_c
    )
};

#include <HXLibs/log/Log.hpp>

int main() { 
    HXTest_1::cou();
    HXTest_2::cou();
    static_assert(reflection::membersCountVal<HXTest_1> != reflection::membersCountVal<HXTest_2>, "pp");
    constexpr auto name1 = reflection::getMembersNames<HXTest_1>();
    constexpr auto name2 = reflection::getMembersNames<HXTest_2>();
    // static_assert(name1 != name2, "sb is you");
    // static_assert(name1[0] == "fk_a", "sb is yy");
    // static_assert(name2[0] == "sb_a", "sb is yyy");
    log::hxLog.debug(name1);
    log::hxLog.debug(name2);
    return 0;
}