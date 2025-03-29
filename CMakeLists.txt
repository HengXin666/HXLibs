cmake_minimum_required(VERSION 3.15)

set(HX_LIBS_VERSION "0.1.1")

# 定义工程名称, 并指定语言为C++
project(HXLibs
    VERSION ${HX_LIBS_VERSION}
    DESCRIPTION "HXLibs"
    HOMEPAGE_URL "https://github.com/HengXin666/HXLibs"
    LANGUAGES CXX
)

# 是否开启 绕过操作系统的缓存, 直接将数据从用户空间读写到磁盘
# set(IO_URING_DIRECT ON) # 有BUG 先不要用

include(cmake/install.cmake)

if(CMAKE_PROJECT_NAME STREQUAL "HXLibs")
    # 如果没有定义CMAKE_BUILD_TYPE, 则设置为Release
    # 可以使用 cmake -DCMAKE_BUILD_TYPE=Debug .. 来指定
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
        message("=-=-=-=-=-=-=当前是 [Release] 模式=-=-=-=-=-=-=")
    else()
        message("=-=-=-=-=-=-=当前是 [DEBUG] 模式=-=-=-=-=-=-=")
        add_definitions(-D_HX_DEBUG_) # 添加一个 _HX_DEBUG_ 宏
        message("已添加宏: _HX_DEBUG_")
    endif()

    include(cmake/utils.cmake)
    include(cmake/config.cmake)
    include(cmake/develop.cmake)
    include(cmake/examples.cmake)
    include(cmake/tests.cmake)
else()
    include(cmake/config.cmake)
endif()