#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXJson/Json.h>
#include <HXprint/print.h>

TEST_CASE("JSON解析") {
    // 宽松解析
    {
        auto [data, err] = HX::json::parse(R"(
            {
                "name": "张三",
                "age": 27,
            }
        )");
        CHECK(data["name"].get<std::string>() == "张三");
        CHECK(data["age"].get<int>() == 27);
    }
    
    // 紧凑解析
    {
        auto [data, err] = HX::json::parse(R"({"name":"张三","age":27})");
        CHECK(data["name"].get<std::string>() == "张三");
        CHECK(data["age"].get<int>() == 27);
    }

    // 任意缩进解析
    {
        auto [data, err] = HX::json::parse(R"({
        "name":                        "张三"                           ,
                
"age":27                                     })");
        CHECK(data["name"].get<std::string>() == "张三");
        CHECK(data["age"].get<int>() == 27);
    }
}

#include <HXJson/JsonWrite.hpp>

TEST_CASE("测试无宏structToJson") {
    // 简单结构体
    {
        struct Man {
            int id;
            std::string name;
        };

        std::string jsonStr;
        HX::json::toJson(Man{1, "张三"}, jsonStr);
        CHECK(jsonStr == R"({"id":1,"name":"张三"})");
    }

    // 嵌套结构体 (const std::vector<自定义结构体>)
    {
        struct CatHome {
            struct Cat {
                int id;
                std::string name;
            };
            const std::vector<Cat> cats;
        };

        CatHome cats {{
            {1, "咪咪"},
            {2, "明卡"},
            {3, "TomCat"}
        }};

        std::string jsonStr;
        HX::json::toJson(cats, jsonStr);
        CHECK(jsonStr == R"({"cats":[{"id":1,"name":"咪咪"},{"id":2,"name":"明卡"},{"id":3,"name":"TomCat"}]})");
    }

    // 复杂的嵌套结构体
    {
        struct Data {
            std::unordered_map<std::string, std::string> nameUidMap;
            std::list<int> idList;
            bool ok;
        };

        struct JsonVo {
            int code;
            const std::string_view message;
            Data data;
        };

        JsonVo jsonVo {
            200,
            "Get Ok!",
            {
                {
                    {"张三", "uuid0x0721"},
                    {"王五", "uuid0x07222"},
                    {"老六", "uuid0x0666"},
                },
                { 1, 1, 4, 5, 1, 4},
                true
            }
        };
        
        std::string jsonStr;
        HX::json::toJson(jsonVo, jsonStr);
        CHECK(jsonStr == R"({"code":200,"message":"Get Ok!","data":{"nameUidMap":{"老六":"uuid0x0666","王五":"uuid0x07222","张三":"uuid0x0721"},"idList":[1,1,4,5,1,4],"ok":true}})");
    }
}

#include <HXJson/JsonRead.hpp>

TEST_CASE("测试无宏structFromJson") {
    // 简单结构体
    {
        struct Man {
            int id;
            std::string name;
        };

        std::string json = R"({"id":1,"name":"张三"})";
        Man man{};
        HX::json::fromJson(man, json);
        HX::print::println(man);
    }

    // 嵌套结构体
    {
        struct CatHome {
            struct Cat {
                int id;
                std::string name;
            };
            std::vector<Cat> cats;
        };

        CatHome cats {};

        HX::json::fromJson(cats, R"({"cats":[{"id":1,"name":"咪咪"},{"id":2,"name":"明卡"},{"id":3,"name":"TomCat"}]})");
        HX::print::println(cats);
    }

    // 复杂的嵌套结构体
    {
        struct Data {
            std::unordered_map<std::string, std::string> nameUidMap;
            std::list<int> idList;
            bool ok;
        };

        struct JsonVo {
            int code;
            std::string message;
            Data data;
        };

        JsonVo jsonVo {};
        HX::json::fromJson(jsonVo, R"({"code":200,"message":"Get Ok!","data":{"nameUidMap":{"老六":"uuid0x0666","王五":"uuid0x07222","张三":"uuid0x0721"},"idList":[1,1,4,5,1,4],"ok":true}})");
        HX::print::println(jsonVo);
    }
}

/**
 * @todo Heng_Xin 还需要补充测试例子!
 * @warning 目前已知: 不能解析超高精度、超大数字 (让他们以字符串形式接收)
 * @warning 目前, 反序列化还不是零拷贝的, 后续需要支持一下(至少是优化!)
 * 但目前无法证明程序能支持所有 JSON 格式内容。为了验证对 JSON 的全面支持，你可以补充以下测试用例：
 * - 添加对基础数据类型（如 double、float、bool、null）的测试。
 * - 验证边界情况（字段缺失、多余字段、类型错误）。
 * - 添加空对象、空数组和嵌套更深的 JSON 测试。
 * - 测试包含特殊字符和大数据的 JSON。
 */

// JSON解析示例
void test_01() {
    auto json = HX::json::parse(R"(
[
  {
    "name": "Molecule Man",
    "age": 29,
    "secretIdentity": "Dan Jukes",
    "powers": ["Radiation resistance", "Turning tiny", "Radiation blast"]
  },
  {
    "name": "Madame Uppercut",
    "age": 39.0000009,
    "secretIdentity": "Jane Wilson",
    "powers": [
      "Million tonne punch",
      "Damage resistance",
      "Superhuman reflexes"
    ],
    sb: null
  }
]
)").first; // .first 是解析的jsonObj, 而 .second 是解析的字符数(如果为 0, 则是解析失败)

    json.print();
    std::cout << '\n';
    
    json.get<HX::json::JsonList>()[1].get<HX::json::JsonDict>()["sb"].print();
    std::cout << '\n';

    json[1]["sb"].print();
    std::cout << '\n';

    json[0]["powers"][0].print();
    std::cout << '\n';

    auto str = json.toString();
    std::cout << str << '\n';
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <HXJson/ReflectJson.hpp> // <-- 反射 宏的头文件: 对外提供的是 以`REFLECT`开头, 以`_`开头的是内部使用的宏

struct Student {
    std::string name;
    int age;
    struct Loli {
        std::string name;
        int age;
        int kawaiiCnt;

        REFLECT_CONSTRUCTOR_ALL(Loli, name, age, kawaiiCnt) // 可以嵌套, 但是也需要进行静态反射(需要实现`toJson`方法)
    };
    std::vector<Loli> lolis;
    std::unordered_map<std::string, std::string> woc;
    std::unordered_map<std::string, Loli> awa;

    // 静态反射, 到时候提供`toJson`方法以序列化为JSON
    // 提供 构造函数(从json字符串和json构造, 以及所有成员的默认构造函数)
    // 注: 如果不希望生成 [所有成员的默认构造函数], 可以使用 REFLECT_CONSTRUCTOR 宏
    REFLECT_CONSTRUCTOR_ALL(Student, name, age, lolis, woc, awa)
};

/// @brief 一个只读的 Json 反射
struct StudentConst {
    const Student stuConts;
    const int& abc;

    // 这个只反射到toJson函数(即序列化为json), 而不能从`jsonStr/jsonObj`构造
    // 你 const auto& 还怎么想从一个临时的jsonObj引用过来? 它本身就不安全, jsonStr就更不用说了!
    REFLECT(stuConts, abc)
};

#include <HXJson/UnReflectJson.hpp> // <-- undef 相关的所有宏的头文件, 因为宏会污染全局命名空间

// JSON 序列化(结构体 toJsonString)示例
void test_02() {
    Student stu { // 此处使用了 宏生成的 [所有成员的默认构造函数] (方便我调试awa)
        "Heng_Xin",
        20,
        {{
            "ラストオーダー",
            13,
            100
        }, {
            "みりむ",
            14,
            100
        }},
        {
            {"hello", "word"},
            {"op", "ed"}
        },
        {
            {"hello", {
                "みりむ",
                14,
                100
            }}
        }
    };
    // 示例: 转化为json字符串(紧凑的)
    HX::print::print(stu.toJson());
    auto json = HX::json::parse(stu.toJson()).first;
    json.print();
    printf("\n\n");

    // 示例: 从json对象 / json字符串转为 结构体

    json["age"] = HX::json::JsonObject {}; // 如果我们修改了它的类型 / 解析不到对应类型

    Student x(json);
    HX::json::parse(x.toJson()).first.print();

    printf("\n\n");
    // 即便是空的也无所谓, 不是json也无所谓, 只是解析到的是空josn对象
    HX::json::parse(Student("Heng_Xin is nb!").toJson()).first.print();
}

void test_03() {
    Student stu { // 此处使用了 宏生成的 [所有成员的默认构造函数] (方便我调试awa)
        "Heng_Xin",
        20,
        {{
            "ラストオーダー",
            13,
            100
        }, {
            "みりむ",
            14,
            100
        }},
        {
            {"hello", "word"},
            {"op", "ed"}
        },
        {
            {"hello", {
                "みりむ",
                14,
                100
            }}
        }
    };

    int abc = 123;
    StudentConst stdConst(stu, abc);

    HX::json::parse(stdConst.toJson()).first.print();

    abc = 567; // 修改了它
    printf("\n修改了abc...\n");
    HX::json::parse(stdConst.toJson()).first.print();
}

// int main () {
//     HX::print::print("示例1: json解析\n");
//     test_01();
//     HX::print::print("\n\n示例2: json合成string || jsonString合成到结构体 || 其他鲁棒性测试示例\n");
//     test_02();
//     HX::print::print("\n\n示例3: 对于 const auto& 类型的 toJsonString 的支持\n");
//     test_03();
//     return 0;
// }