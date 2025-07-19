# C++代码规范
> ~~Heng_Xin 自用~~

> [!NOTE]
> 本项目 C++ 支持最低 C++20 的版本, 应该尝试使用更加现代的写法!

文件命名规范: 驼峰命名

- `.hpp` 表示里面有模板; `.h` 是没有模板的

- `#include`规范:

```cpp
#include <HXLibs/net/socket/AddressResolver.h>  // .cpp 与 .h 配对 (然后空行)
                                                // 本库纯头文件, 这一行可忽略

#include <sys/types.h>  // 标准库/Linux提供的头文件 (.h在上, 无后缀的在下)
#include <sys/socket.h> // 理论上会封装到 HXLibs/platform/**Api.hpp 中 (平台相关的api)
#include <netdb.h>
#include <cstring>
#include <iostream>     // (然后空行)

#include <openssl/bio.h> // 第三方库
#include <openssl/ssl.h>  
#include <openssl/err.h> // (然后空行)

#include <HXLibs/platform/SocketApi.hpp>        // 本项目为了跨平台而封装的API (然后空行)

#include <HXLibs/coroutine/awaiter/WhenAny.hpp> // 本项目的 库文件
```

- 命名空间规范: 按照文件夹来, 比如`/HXLibs/coroutine/loop/IoUringLoop.h`

```cpp
namespace HX::coroutine { // 二级文件夹是模块名称, 其内部子模块共用二级命名空间

// 此处是库的某些内部实现
namespace internal {

} // namespace internal

} // namespace HX::coroutine
```

- 类成员命名方式为`_name`(自用函数也是以`_`开头)
- 变量名: 几乎全部都是`驼峰`命名
- 枚举: 首字母大写的驼峰
- 编译期常量: (`k`开头 +) 驼峰 或者 首字母大写的驼峰

- 宏定义: 全大写+下划线

> [!TIP]
> 缩进为`4空格`, 并且TAB按键请使用`4空格`而不是`\t`!