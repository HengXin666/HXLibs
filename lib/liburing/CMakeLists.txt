# 如果需要使用到 <liburing.h>, 则需要添加以下代码
# 因为本库默认开启了很强的编译检测, 而这个库使用了匿名结构体
# 因此, 我们需要暂时的屏蔽警告: -Wpedantic

# ------------------- 例如 ------------------- {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#include <liburing.h>

#pragma GCC diagnostic pop

# } ------------------- 例如 -------------------

# 创建 liburing 静态库
add_library(liburing STATIC liburing.cpp)

# 包含头文件目录, 使用 BUILD_INTERFACE 和 INSTALL_INTERFACE
target_include_directories(liburing PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>   # 构建时使用源码目录中的 include
    $<INSTALL_INTERFACE:include>                             # 安装后使用安装目录中的 include
)

# 将 liburing 添加到导出集合中
export(TARGETS liburing FILE "${CMAKE_CURRENT_BINARY_DIR}/liburingTargets.cmake")

# 导出 liburing 目标
install(TARGETS liburing EXPORT liburingTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 生成 CMake 配置文件, 供 find_package 使用
install(EXPORT liburingTargets
    FILE liburingTargets.cmake
    NAMESPACE liburing::
    DESTINATION lib/cmake/liburing
)