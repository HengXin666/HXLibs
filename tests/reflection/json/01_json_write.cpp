#include <HXLibs/reflection/json/JsonWrite.hpp>
#include <HXLibs/reflection/json/JsonRead.hpp>
#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

struct Cat {
    std::string name;
    int num;
    std::vector<Cat> sub;

    bool operator==(Cat const& that) const {
        return name == that.name
            && num == that.num
            && sub == that.sub;
    }
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
    Cat catCat;
    reflection::fromJson(catCat, catStr);
    CHECK(cat == catCat);
}

TEST_CASE("枚举") {
    enum class Enum : uint32_t {
        A, B, C, D, E, F, G
    };

    struct EnumTest {
        std::array<Enum, 4> arr;

        bool operator==(EnumTest const& that) const {
            for (std::size_t i = 0; i < arr.size(); ++i)
                if (arr[i] != that.arr[i])
                    return false;
            return true;
        }
    };

    EnumTest e{Enum::A, Enum::C, Enum::E, Enum::G};
    std::string jsonStr;
    reflection::toJson(e, jsonStr);

    EnumTest ee;
    reflection::fromJson(ee, jsonStr);
    CHECK(e == ee);
}

TEST_CASE("带双引号的字符串josn") {
    struct A {
        std::string name;

        bool operator==(A const& a) const {
            return name == a.name;
        }
    };
    std::string json;
    A a{
        "\"张三\": Hello?"
    };
    reflection::toJson(a, json);
    log::hxLog.info(json);

    A aa;
    reflection::fromJson(aa, json);
    CHECK(a == aa);
}