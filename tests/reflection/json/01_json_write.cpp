#include <HXLibs/reflection/json/JsonWrite.hpp>
#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

struct Cat {
    std::string name;
    int num;
    std::vector<Cat> sub;
};

using namespace HX;

TEST_CASE("json 序列化") {
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
    std::string catStr;
    reflection::toJson(cat, catStr);
    log::hxLog.info("元素:", cat);
    log::hxLog.info("序列化字符串:", catStr);
    CHECK(catStr == R"({"name":"咪咪","num":2233,"sub":[{"name":"明卡","num":233,"sub":[{"name":"大猫","num":666,"sub":[]}]},{"name":"喵喵","num":114514,"sub":[{"name":"","num":0,"sub":[]}]}]})");
    catStr.clear();
    reflection::toJson<true>(cat, catStr);
    log::hxLog.info("格式化的:", catStr);
}