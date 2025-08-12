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
        std::string data(114514, 'H');
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
    {
        utils::TickTock<> _{"协程的同步"};
        utils::AsyncFile file{loop};
        loop.sync(file.open("./01_file.test"));
        res01 = file.syncReadAll();
        file.syncClose();
    }
    {
        utils::TickTock<> _{"同步"};
        res02 = utils::FileUtils::getFileContent("./01_file.test");
    }
    bool isEq = res01 == res02;
    log::hxLog.debug(isEq);
    CHECK(isEq);
}