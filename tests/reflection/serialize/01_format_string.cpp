#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/log/serialize/FormatString.hpp>

using namespace HX;

#include <list>
#include <unordered_map>
#include <span>

#define HX_TEST_INFO(val, goodVal)                  \
    std::cout << log::formatString(val) << '\n';    \
    CHECK(log::formatString(val) == std::string{goodVal})

TEST_CASE("格式化 arr / 数字 / bool") {
    std::vector<int> arr = {1, 2, 3, 4, 5};

    HX_TEST_INFO(arr, "[1, 2, 3, 4, 5]");

    double arr02[] = {
        1.1,
        2.2,
        3.14,
        4.28,
        5.16,
        1e9 + 7,
        -3e-17,
        1.0000000000000000000001,
        1.000000000000000000001,
        1.00000000000000000001,
        1.0000000000000000001,
        1.000000000000000001,
        1.00000000000000001,
        1.0000000000000001,
        1.000000000000001,
        1.00000000000001,
        1.0000000000001,
        1.000000000001,
        1.00000000001,
        1.0000000001,
        1.000000001,
        1.00000001,
        1.0000001,
        1.000001,
    };

    std::string goodVal = "[1.1, 2.2, 3.14, 4.28, 5.16, 1000000007, -3e-17, 1, 1, 1, 1, 1, 1, 1, 1.000000000000001, 1.00000000000001, 1.0000000000001, 1.000000000001, 1.00000000001, 1.0000000001, 1.000000001, 1.00000001, 1.0000001, 1.000001]";

    HX_TEST_INFO(arr02, goodVal);
    HX_TEST_INFO(std::span<double>{arr02}, goodVal);

    CHECK(log::formatString(true) == "true");
    CHECK(log::formatString(false) == "false");
}

TEST_CASE("格式化 kv") {
    std::map<std::string, std::unordered_map<std::string, std::list<int>>> kv = {
        {
            "one", 
            {
                {"战士", {1, 4, 3, 3, 2, 2, 3}},
                {"骑士", {3, 14, 15, 926, 535897836}}
            }
        },
        {
            "tow", 
            {
                {"战士", {1, 4, 3, 3, 2, 2, 3}},
                {"骑士", {3, 14, 15, 926, 535897836}}
            }
        }
    };
    HX_TEST_INFO(kv, R"({
    "one": {
        "骑士": [3, 14, 15, 926, 535897836],
        "战士": [1, 4, 3, 3, 2, 2, 3]
    },
    "tow": {
        "骑士": [3, 14, 15, 926, 535897836],
        "战士": [1, 4, 3, 3, 2, 2, 3]
    }
})");
}

TEST_CASE("格式化 聚合类") {
    struct Cat {
        std::string name;
        int num;
        std::vector<Cat> sub;
    };
    Cat cat = {
        "咪咪",
        2233,
        {{
            "明卡",
            233,
            {{
                "大猫",
                666,
                {}
            }}
        }, {
            "喵喵",
            114514,
            {{}}
        }}
    };
    HX_TEST_INFO(cat, R"({
    "name": "咪咪",
    "num": 2233,
    "sub": [{
        "name": "明卡",
        "num": 233,
        "sub": [{
            "name": "大猫",
            "num": 666,
            "sub": []
        }]
    }, {
        "name": "喵喵",
        "num": 114514,
        "sub": [{
            "name": "",
            "num": 0,
            "sub": []
        }]
    }]
})");
}

TEST_CASE("序列化 tuple / pair") {
    std::pair<int, std::string> p2{2233, "2233"};
    HX_TEST_INFO(p2, R"((2233, "2233"))");

    std::tuple<std::pair<int, std::string>, int, int, int> t4 {
        {2233, "2233"}, 1, 2, 3
    };
    HX_TEST_INFO(t4, R"(((2233, "2233"), 1, 2, 3))");
}

TEST_CASE("序列化 指针 / null") {
    int x = 114514;
    int* p = &x;
    int** pp = &p;
    std::cout << log::formatString(p) << '\n';
    std::cout << log::formatString(pp) << '\n';
    std::cout << log::formatString(NULL) << '\n'; // 是 0
    std::cout << log::formatString((void*)NULL) << '\n'; // 是 0x0

    CHECK(log::formatString(nullptr) == "null");
    CHECK(log::formatString(std::nullopt) == "null");
    CHECK(log::formatString(std::monostate{}) == "null");
}

TEST_CASE("序列化 variant") {
    std::variant<int, double, std::vector<std::string>> u;
    CHECK(log::formatString(u) == "0");
    u = 123;
    CHECK(log::formatString(u) == "123");
    u = 123.456;
    CHECK(log::formatString(u) == "123.456");
    u = std::vector<std::string>{"3.14", "qwq", "awa"};
    CHECK(log::formatString(u) == R"(["3.14", "qwq", "awa"])");
}

TEST_CASE("test") {
    struct DanmakuElem {
        int64_t id;               // 弹幕id
        int32_t progress;         // 出现时间（单位 ms）
        int32_t mode;             // 弹幕类型
        int32_t fontsize;         // 字号
        uint32_t color;           // 颜色（RGB）
        std::string midHash;      // 发送者mid hash
        std::string content;      // 弹幕内容
        int64_t ctime;            // 发送时间
        int32_t weight;           // 权重
        std::string action;       // 动作
        int32_t pool;             // 弹幕池（0普通池）
        std::string idStr;        // 弹幕id str
        int32_t attr;             // 属性位（如保护/高赞等）
        std::string animation;    // 动画（暂未使用）
        std::optional<int32_t> colorful; // 大会员渐变色（可选）
    };

    DanmakuElem dm {
        1, 2, 3, 4, 5, 
        "6", "7", 8, 9, "10", 
        11 ,"12", 13, "14", {16}
    };

    std::cout << log::formatString(dm) << '\n';

    struct HttpHeader {
        std::string_view name;
        std::string_view value;
    };

    std::vector<HttpHeader> hhArr = {
        {"1", "2"},
        {"1", "2"},
        {"1", "2"}
    };
    std::span<HttpHeader> spHH = hhArr;
    std::cout << log::formatString(spHH) << '\n';
}