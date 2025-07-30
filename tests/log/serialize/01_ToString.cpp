#include <HXLibs/log/serialize/ToString.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX;

#include <vector>
#include <list>
#include <map>

TEST_CASE("测试紧凑的序列化") {
    std::cout << log::toString("1_abc_我🌿", 1, 1.11, '\n', true, false, nullptr, std::nullopt, '\n');
}

TEST_CASE("测试序列化 List") {
    std::vector<std::list<int>> arr2D{{1, 2, 3}, {4, 5, 6}};
    std::cout << log::toString(arr2D, '\n');

    std::string buf;
    log::toString(arr2D, buf);
    CHECK(buf == log::toString(arr2D));
}

TEST_CASE("测试序列化 Map / 结构体 / tuple / variant") {
    struct Test {
        int v;
        std::vector<std::map<std::string, int>> mmp;
        std::tuple<std::variant<double, int, std::string>, std::optional<std::vector<std::map<std::string, int>>>> tp;
    };

    // 得显式写明聚合构造, 不然编译器就激进担忧而警告而报错
    Test test{
        1,
        {
            {{"op", 1}, {"ed", 2}},
            {{"OP", 11}, {"ED", 2}}
        },
        {
            std::variant<double, int, std::string>{3.14},
            std::optional<std::vector<std::map<std::string, int>>>{
                std::vector<std::map<std::string, int>>{{}, {}}
            }
        }
    };
    std::cout << log::toString(test, '\n');

    std::string buf;
    log::toString(test, buf);
    CHECK(buf == log::toString(test));
}

TEST_CASE("测试 wchar_t") {
    std::cout << log::toString(L"123_😦😧😮😲😍ロリ\n");
    CHECK(log::toString(L"123_😦😧😮😲😍ロリ\n") == "123_😦😧😮😲😍ロリ\n");

    wchar_t wstr[] = L"🈚️🌿🍵飒沓⬆️✅𤼵𪍹";
    std::cout << log::toString(wstr, '\n');

    std::string buf;
    log::toString(wstr, buf);
    for (std::size_t i = 0; i < buf.size(); ++i) {
        CHECK(buf[i] == i["🈚️🌿🍵飒沓⬆️✅𤼵𪍹"]);
    }
    std::string ck{"🈚️🌿🍵飒沓⬆️✅𤼵𪍹"};
    CHECK(ck.size() == buf.size());
}