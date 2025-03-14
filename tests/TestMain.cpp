#include <iostream>

/*
 * Copyright Heng_Xin. All rights reserved.
 *
 * @Author: Heng_Xin
 * @Date: 2024-7-31 12:13:49
 * @brief: 测试程序, 用于测试代码!
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * */

#include <functional>
#include <HXSTL/container/RadixTree.hpp>
/// @brief 压缩前缀树测试
void testRadixTree() {
    using func = std::function<void()>;
    HX::STL::container::RadixTree<func> tree;
    tree.insert({"awa", "qwq", "*"}, [](){
        printf("awa");
    });
    tree.insert({"awa", "0.0", "*"}, [](){
        printf("qwq");
    });
    (*tree.find({"awa", "0.0", "*"}))();
    if (!tree.find({"qwq", "0.0"}).has_value())
        printf("\n没有这个\n");
}

#include <HXSTL/utils/MagicEnum.hpp>
/// @brief 魔法枚举测试
void testMagicEnum() {
    enum MyEnum {
        LoLi = 1,
        imouto = 8,
    };
    // std::cout << HX::STL::utils::MagicEnum::getEnumName<MyEnum>(MyEnum::imouto) << '\n';
    // std::cout << HX::STL::utils::MagicEnum::nameFromEnum<MyEnum>("imouto") << '\n';
}

#include <chrono>

using namespace std::chrono;

#include <HXSTL/coroutine/loop/AsyncLoop.h>
#include <HXSTL/coroutine/task/Task.hpp>
#include <HXSTL/coroutine/task/TimerTask.h>
#include <HXSTL/utils/FileUtils.h>

struct Log {
    Log(std::string msg)
        : _msg(msg)
    { std::cout << _msg << " {\n"; }

    ~Log() {
        std::cout << _msg << " }\n";
    }

    std::string _msg;
};

HX::STL::coroutine::task::Task<> B() {
    Log _("B");
    co_await HX::STL::coroutine::loop::TimerLoop::sleepFor(1s);
}

HX::STL::coroutine::task::Task<> A() {
    Log _("A");
    co_await HX::STL::coroutine::loop::TimerLoop::sleepFor(1s);
    co_await B();
    co_await HX::STL::coroutine::loop::TimerLoop::sleepFor(1s);
}

HX::STL::coroutine::task::TimerTask timerTask(int x) {
    printf("%d, SB\n", x);
    {
        Log _("test");
        co_await A();
    }
    std::string file = co_await HX::STL::utils::FileUtils::asyncGetFileContent("index.html");
    std::cout << file.size() << '\n';
    co_await HX::STL::coroutine::loop::TimerLoop::sleepFor(1s);
    printf("%d, 大SB!\n", x);
    co_return;
}

HX::STL::coroutine::task::Task<> taskMain() {
    printf("你好!\n");
    // {
    //     Log _("test");
    //     co_await A();
    // }
    int zz = 123;
    HX::STL::coroutine::loop::AsyncLoop::getLoop().getTimerLoop().addInitiationTask(
        std::make_shared<HX::STL::coroutine::task::TimerTask>(timerTask(zz))
    );
    co_await HX::STL::coroutine::loop::TimerLoop::sleepFor(2s);
    printf("好好说话!!\n");
    co_return;
}

int main() {
    chdir("../static");
    HX::STL::coroutine::task::runTask(
        HX::STL::coroutine::loop::AsyncLoop::getLoop(),
        taskMain()
    );
    return 0;
}

// 测试代码, 下面发现了一个潜在的问题
#include <HXSTL/coroutine/task/WhenAny.hpp>
#include <chrono>

HX::STL::coroutine::task::Task<bool> __text() {
    co_return false;
}

HX::STL::coroutine::task::Task<bool> _text() {
    // co_await HX::STL::coroutine::loop::TimerLoop::sleepFor(std::chrono::seconds{0});
    co_return co_await __text();
}

HX::STL::coroutine::task::Task<> test() {
    co_await HX::STL::coroutine::task::WhenAny::whenAny( // 如果下面参数互换, 则会段错误
        HX::STL::coroutine::loop::TimerLoop::sleepFor(std::chrono::seconds{3}),
        _text()
    );
}