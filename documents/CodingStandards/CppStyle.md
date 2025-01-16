# C++代码规范
> ~~Heng_Xin 自用~~

- 模块根文件夹使用驼峰(如`HXWeb`), 分类的子文件夹使用小写(如`tools`), 文件使用驼峰命名.
    - 其中纯头文件(一般里面只有模版等必须只能写在头文件的), 以`.hpp`结尾
    - 剩下的, 实现的代码可以放在`.cpp`里面的, 有`.h`配对
    - `.h`头文件里面应该尽量少的`#include`, 而尽量在`.cpp`里面引入

- `#include`规范:
```cpp
#include <HXWeb/socket/AddressResolver.h> // .cpp 与 .h 配对 (然后空行)

#include <sys/types.h>  // 标准库/Linux提供的头文件 (.h在上, 无后缀的在下)
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <iostream>     // (然后空行)

#include <openssl/bio.h> // 第三方库
#include <openssl/ssl.h>  
#include <openssl/err.h> // (然后空行)

#include <HXSTL/tools/ErrorHandlingTools.h> // 本项目的 库文件
```

- 命名空间规范: 按照文件夹来, 比如`/HXSTL/coroutine/loop/IoUringLoop.h`, 其中(`HXSTL`是模块根文件夹, 其意思是`HX::STL`)
```cpp
namespace HX { namespace STL { namespace coroutine { namespace loop {

// 此处是库的某些内部实现
namespace internal {

} // namespace internal

}}}} // namespace HX::STL::coroutine::loop
```

- 类成员命名方式为`_name`(自用函数也是以`_`开头)
- 变量名: 几乎全部都是`驼峰`命名
- 枚举: 首字母大写的驼峰
- 编译期常量: (`k`开头 +) 驼峰
- ~~类的构造函数: 请 **显式** 书写构造函数, 必要可以添加`explicit`关键字!, 而不是依赖`C++20`的自动隐式匹配生成!~~
    - 聚合类可以!!!

- 宏定义: 全大写+下划线

> [!TIP]
> 缩进为`4空格`, 并且TAB按键请使用`4空格`而不是`\t`!