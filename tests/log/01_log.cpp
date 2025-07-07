#include <HXLibs/log/Log.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

using namespace HX::log;

TEST_CASE("dbg") {
    hxLog.debug("=== debug test ===");
    hxLog.debug("string:", std::string{"字符串"});
    hxLog.debug("tuple:", std::tuple<int, void*, std::optional<int>>{1, nullptr, {}});
}


TEST_CASE("ifo") {
    hxLog.info("=== info test ===");
    hxLog.info("string:", std::string{"字符串"});
    hxLog.info("tuple:", std::tuple<int, void*, std::optional<int>>{1, nullptr, {}});
}

TEST_CASE("war") {
    hxLog.warning("=== warning test ===");
    hxLog.warning("string:", std::string{"字符串"});
    hxLog.warning("tuple:", std::tuple<int, void*, std::optional<int>>{1, nullptr, {}});
}

TEST_CASE("err") {
    hxLog.error("=== error test ===");
    hxLog.error("string:", std::string{"字符串"});
    hxLog.error("tuple:", std::tuple<int, void*, std::optional<int>>{1, nullptr, {}});
}