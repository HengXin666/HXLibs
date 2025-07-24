#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/log/Log.hpp>

#include <string_view>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/container/CHashMap.hpp>

#include <span>

using namespace HX;

TEST_CASE("CHashMap") {
    constexpr container::CHashMap<std::string_view, std::size_t, 4> mp{
        {
            std::pair<std::string_view, std::size_t>{"1", 1},
            {"2", 1},
            {"3", 1},
            {"4", 1},
        }
    };

    static_assert(mp.at("1") == 1, "111");
    static_assert(mp.find("2") != mp.end(), "111");
}

TEST_CASE("json") {
    struct AJson {
        bool a;
        std::vector<std::vector<std::string>> b;
        std::unordered_map<std::string, std::string> c;
        std::optional<std::string> d;
        std::shared_ptr<std::string> e;
        double f;
    } t{};

    reflection::fromJson(t, R"({
        "f" : -1.23E-10,
        "d" : "1433223",
        "a" : 1,
        "c" : {
            "不支持注释!" : "!",
            "" : "
            "
        },
        "b" : [
            ["2233", "a\\r\\rb\"awsasdas"], 
            [],
            [""]
        ],   
        "e" : nuLL
    })");
}

TEST_CASE("json_all_case") {
    struct A {
        bool a;
        std::vector<std::vector<std::string>> b;
        std::unordered_map<std::string, std::string> c;
        std::optional<std::string> d;
        std::shared_ptr<std::string> e;
        double f;
    } t{};

    reflection::fromJson(t, R"({
        "f": -3.1415926E+10,
        "d": null,
        "a": 0,
        "c": {
            "": "",
            "中文Key": "中文Value",
            "特殊字符!@#$%^&*()_+": "特殊Value",
            "换行": "第一行\n第二行",
            "制表符": "\tTabTest"
        },
        "b": [
            [],
            ["单独一个"],
            ["多行\n字符串", "特殊字符\"\\\'", "空字符串", "\u4E2D\u6587"]
        ],
        "e": "HelloPtr"
    })");

    // 断言检查
    REQUIRE(t.a == false);
    REQUIRE(t.f == -3.1415926E+10);
    REQUIRE(!t.d.has_value());
    REQUIRE(t.e != nullptr);
    REQUIRE(*t.e == "HelloPtr");

    REQUIRE(t.c.at("") == "");
    REQUIRE(t.c.at("中文Key") == "中文Value");
    REQUIRE(t.c.at("特殊字符!@#$%^&*()_+") == "特殊Value");
    REQUIRE(t.c.at("换行") == "第一行\n第二行");
    REQUIRE(t.c.at("制表符") == "\tTabTest");

    REQUIRE(t.b.size() == 3);
    REQUIRE(t.b[0].empty());
    REQUIRE(t.b[1].size() == 1);
    REQUIRE(t.b[1][0] == "单独一个");
    REQUIRE(t.b[2].size() == 4);
    REQUIRE(t.b[2][0] == "多行\n字符串");
    REQUIRE(t.b[2][1] == "特殊字符\"\\\'");
    REQUIRE(t.b[2][2] != "");
    REQUIRE(t.b[2][3] == "中文");
}

TEST_CASE("json_read_array") {
    struct A {
        std::array<double, 10> arr{};
        std::span<double> span = arr;
    } t;
    reflection::fromJson(t, R"(
    {
        "arr": [1, 9.1, 10e-10],
        "span": []
    }
    )");
}