#include <HXLibs/utils/FileUtils.hpp>
#include <HXLibs/log/Log.hpp>
#include <HXLibs/utils//TickTock.hpp>

using namespace HX;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

TEST_CASE("测试朴素同步读写文件接口测试") {
    coroutine::EventLoop loop{};
    // 写
    {
        utils::AsyncFile file{loop};
        loop.sync(file.open("./01_file.test"));
        std::string data(1 << 20, 'H');
        loop.sync(file.write(data));
        file.syncClose();
    }
    // 读取
    {
        utils::AsyncFile file{loop};
        loop.sync(file.open("./01_file.test"));
        std::string data(2233, '_');
        loop.sync(file.read(data));
        for (char c : data) {
            if (c != 'H') {
                CHECK(false);
            }
        }
        file.syncClose();
    }
    CHECK(true);
}

TEST_CASE("性能对比测试") {
    coroutine::EventLoop loop{};
    std::string res01, res02;
    auto coCnt = utils::TickTock<>::forEach(100, [&] {
        utils::AsyncFile file{loop};
        loop.sync(file.open("./01_file.test"));
        res01 = file.syncReadAll();
        file.syncClose();
    });
    log::hxLog.info("协程的同步:", coCnt);
    auto syncCnt = utils::TickTock<>::forEach(100, [&] {
        res02 = utils::FileUtils::getFileContent("./01_file.test");
    });
    log::hxLog.info("系统调用:", syncCnt);
    log::hxLog.warning("差距:", syncCnt / coCnt, "倍");
    CHECK(res01.size() == res02.size());
    bool isEq = res01 == res02;
    CHECK(isEq);
    log::hxLog.debug(isEq);
}

TEST_CASE("只读打开文件: 不会自动创建") {
    coroutine::EventLoop loop{};
    utils::AsyncFile file{loop};
    try {
        file.syncOpen("./xxxxx.xxx", utils::OpenMode::Read);
        CHECK(false);
        file.syncClose();
    } catch (...) {
        CHECK(true);
    }
}