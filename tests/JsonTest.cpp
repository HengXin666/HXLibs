#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <list>

#include <HXJson/Json.h>
#include <HXprint/print.h>
#include <HXSTL/utils/ToString.hpp>

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
        // HX::print::println(man);
        CHECK(HX::STL::utils::toString(man) == R"({"id":1,"name":"张三"})");
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
        // HX::print::println(cats);
        CHECK(HX::STL::utils::toString(cats) == R"({"cats":[{"id":1,"name":"咪咪"},{"id":2,"name":"明卡"},{"id":3,"name":"TomCat"}]})");
    }

    // 复杂的嵌套结构体
    {
        struct JsonVo {
            int code;
            std::string message;

            // 支持内嵌为`JsonVo::Data`
            struct Data {
                std::unordered_map<std::string, std::string> nameUidMap;
                std::list<int> idList;
                bool ok;
            } data;
        };

        JsonVo jsonVo {};
        HX::json::fromJson(jsonVo, R"({"code":200,"message":"Get Ok!","data":{"nameUidMap":{"老六":"uuid0x0666","王五":"uuid0x07222","张三":"uuid0x0721"},"idList":[1,1,4,5,1,4],"ok":true}})");
        // HX::print::println(jsonVo);
        CHECK(HX::STL::utils::toString(jsonVo) == R"({"code":200,"message":"Get Ok!","data":{"nameUidMap":{"老六":"uuid0x0666","王五":"uuid0x07222","张三":"uuid0x0721"},"idList":[1,1,4,5,1,4],"ok":true}})");
    }
}

/**
 * @todo Heng_Xin 还需要补充测试例子!
 * - 添加对基础数据类型（如 float、null）的测试。
 * - 验证边界情况（字段缺失、多余字段、类型错误）。
 * - 添加空对象、空数组和嵌套更深的 JSON 测试。
 * - 测试包含特殊字符和大数据的 JSON。
 */

TEST_CASE("测试bigNum情况") {
    // 9'223'372'036'854'775'807LL
    // long long 测试
    {    
        struct DataLL {
            long long num;
        };
        DataLL data;
        HX::json::fromJson(data, "{\"num\": 9223372036854775807}");
        // HX::print::println(data);

        CHECK(data.num == 9223372036854775807LL);

        // 示例: 解析数字溢出: 抛异常
        try {
            HX::json::fromJson(data, "{\"num\": 92233720368547758071}");
        } catch (const std::system_error& err) {
            // std::cerr << err.what() << '\n';
        }
    }

    // double 测试
    {
        struct DataD {
            double num;
        };
        DataD data;

        // 精度丢失示例
        HX::json::fromJson(data, "{\"num\": 9223372036854775807}");
        // HX::print::println(data);

        // 正常的
        HX::json::fromJson(data, "{\"num\": 3.1415926535897936}");
        CHECK(data.num == 3.1415926535897936);
        // HX::print::println(data);
    }

    // bigString
    {
        struct DataStr {
            std::string num;
        };
        DataStr data;
        HX::json::fromJson(data, "{\"num\": 112233445566778899}");
        // HX::print::println(data);

        // 解析为字符串, 而不是任何基础数字类型
        CHECK(data.num == "112233445566778899");
    }
}

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

TEST_CASE("复杂json解析") {
    {
        auto json = HX::json::parse(R"(
        [{
            "name": "Molecule Man",
            "age": 29,
            "secretIdentity": "Dan Jukes",
            "powers": ["Radiation resistance", "Turning tiny", "Radiation blast"]
        }, {
            "name": "Madame Uppercut",
            "age": 39.0000009,
            "secretIdentity": "Jane Wilson",
            "powers": [
                "Million tonne punch",
                "Damage resistance",
                "Superhuman reflexes"
            ],
            sb: null
        }])").first; // .first 是解析的jsonObj, 而 .second 是解析的字符数(如果为 0, 则是解析失败)

        // json.print();
        CHECK(json.toString() == R"([{"powers":["Radiation resistance","Turning tiny","Radiation blast"],"secretIdentity":"Dan Jukes","age":29,"name":"Molecule Man"},{"sb":null,"powers":["Million tonne punch","Damage resistance","Superhuman reflexes"],"secretIdentity":"Jane Wilson","age":39.0000009,"name":"Madame Uppercut"}])");
        
        // json.get<HX::json::JsonList>()[1].get<HX::json::JsonDict>()["sb"].print();
        // json[1]["sb"].print();
        CHECK(json.get<HX::json::JsonList>()[1].get<HX::json::JsonDict>()["sb"] == json[1]["sb"]);

        // json[0]["powers"][0].print();
        CHECK(json[0]["powers"][0].get<std::string>() == "Radiation resistance");
    }
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

TEST_CASE("JSON 序列化/反序列化 (使用宏)") {
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
    // HX::print::println(stu.toJson());
    auto json = HX::json::parse(stu.toJson()).first;
    // json.print();

    // 示例: 从json对象 / json字符串转为 结构体
    json["age"] = HX::json::JsonObject {}; // 如果我们修改了它的类型 / 解析不到对应类型

    try {
        Student x(json);
        HX::json::parse(x.toJson()).first.print();
        CHECK(false);
    } catch (const std::exception& err) {
        // HX::print::println("解析失败: ", err.what());
        CHECK(true);
    }
    
    // 解析错误, 则会抛出异常
    try {
        HX::json::parse(
            Student("Heng_Xin is nb!") // 此处 反序列化 失败
            .toJson()
        ).first.print();
        CHECK(false);
    } catch (const std::exception& err) {
        // HX::print::println("解析失败: ", err.what());
        CHECK(true);
    }

    json = HX::json::parse(stu.toJson()).first;

    // 按照const& 传入json, 是拷贝其中的内容
    try {
        Student x(json);
        auto [jsonObj, size] = HX::json::parse(x.toJson());
        // jsonObj.print();
        CHECK(true);
    } catch (const std::exception& err) {
        HX::print::println("解析失败: ", err.what());
        CHECK(false);
    }

    // 按照&& 传入json, 为零拷贝 (对于std::string&& 与 JsonObject&&都有效)
    try {
        Student x(std::move(json));
        auto [jsonObj, size] = HX::json::parse(x.toJson());
        // jsonObj.print();
        // 此时 原本的json 已经为空 (当然, 某些东西不支持移动)
        CHECK(json.toString() == R"({"woc":{},"lolis":[],"awa":{},"age":20,"name":""})");
    } catch (const std::exception& err) {
        HX::print::println("解析失败: ", err.what());
    }
}

TEST_CASE("JSON 序列化(有宏) const auto& 数据") {
    Student stu {
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
    int abc = 123; // 此处传入的 abc 是引用 (const int& abc)
    StudentConst stdConst(stu, abc);

    // HX::json::parse(stdConst.toJson()).first["abc"].print();
    CHECK(HX::json::parse(stdConst.toJson()).first["abc"].toString() == "123");
    abc = 567; // 修改了它

    // HX::json::parse(stdConst.toJson()).first["abc"].print();
    CHECK(HX::json::parse(stdConst.toJson()).first["abc"].toString() == "567");
}