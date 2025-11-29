#include <HXLibs/net/token/Token.hpp>
#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX;
using namespace net;
using namespace log;

TEST_CASE("base64url") {
    struct TestCase {
        std::string original;
        std::string expectedBase64Url;
    };
    std::vector<TestCase> testCases = {
        {"", ""},
        {"f", "Zg"},
        {"fo", "Zm8"},
        {"foo", "Zm9v"},
        {"foob", "Zm9vYg"},
        {"fooba", "Zm9vYmE"},
        {"foobar", "Zm9vYmFy"},
        {"\x01\x02\x03", "AQID"},
        {"Hello, World!", "SGVsbG8sIFdvcmxkIQ"},
        {"Base64-URL test!", "QmFzZTY0LVVSTCB0ZXN0IQ"},
        {"中文测试", "5Lit5paH5rWL6K-V"} // Unicode UTF-8 编码
    };
    for (auto&& test : testCases) {
        auto encode = base64UrlEncode(test.original);
        CHECK(encode == test.expectedBase64Url);
        auto decode = base64UrlDecode(test.expectedBase64Url);
        CHECK(decode == test.original);
    }
}

TEST_CASE("token") {
    struct Cat {
        uint64_t id;
        std::string name;
        std::vector<Cat> fs;

        bool operator==(Cat const& that) const noexcept {
            return id == that.id
                && name == that.name
                && fs == that.fs;
        }
    };

    auto& api = token::TokenApi::get();
    auto const kCat = Cat{
        114514,
        "小猫",
        {
            {
                22,
                "loli",
                {}
            }, {
                33,
                "imouto",
                {}
            }
        }
    };
    auto tk = api.toToken(kCat);
    log::hxLog.info(tk);
    Cat cat{};
    api.fromToken(cat, tk);
    log::hxLog.info(cat);
    CHECK(kCat == cat);
}