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
  - 宏魔法的公共宏为 `HX_{NAME}` 命名
  - 宏魔法的私有宏为 `_hx_MACRO_{NAME}__` 命名
  - 非宏魔法, 仅作为使用的宏函数的宏, 以 `_hx_{NAME}__` 命名

如果 期望使用 `__` 或者 `_大写字母开头`, 应该修改为 `_hx_` 开头. 作为内部私有使用. (特别是全局命名空间)

应该使用 clang 编译一次, 查看是否使用了 保留字 声明.

```cmake
# 检查是否使用了保留字命名
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    add_compile_options(
        -Wpedantic
        -Wreserved-id-macro      # 宏保留前缀
        -Wreserved-identifier    # Clang 检测保留标识符
    )
endif()
```

> [!TIP]
> 缩进为`4空格`, 并且TAB按键请使用`4空格`而不是`\t`!

> 使用 `#pragma once` 作为防止头文件重定义的命令, 不需要定义头文件宏
>
> 可以使用以下代码, 一键删除 头文件的 #if #define #endif 那些防止重定义的条件编译.

```py
import pathlib
import re

GUARD_PATTERN = re.compile(r"_HX_[A-Z0-9_]+_H_")

for header in pathlib.Path("./include").rglob("*.hpp"):
    lines = header.read_text().splitlines()
    new_lines = []

    for line in lines:
        stripped = line.strip()
        # 严格匹配 #ifndef 和 #define
        if (stripped.startswith("#ifndef") or stripped.startswith("#define")):
            parts = stripped.split()
            if len(parts) == 2 and GUARD_PATTERN.fullmatch(parts[1]):
                continue  # 跳过这行宏
        # 严格匹配 #endif 注释行
        if stripped.startswith("#endif // !"):
            macro = stripped[len("#endif // !"):].strip()
            if GUARD_PATTERN.fullmatch(macro):
                continue  # 跳过这行宏
        # 其他行保留
        new_lines.append(line)

    # 在文件头添加 #pragma once（如果不存在）
    if not new_lines or new_lines[0].strip() != "#pragma once":
        new_lines.insert(0, "#pragma once")

    header.write_text("\n".join(new_lines) + "\n")
    print(f"Processed {header}")
```