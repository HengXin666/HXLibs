if (IO_URING_DIRECT)
    target_compile_definitions(HXLibs PUBLIC IO_URING_DIRECT)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(PkgConfig REQUIRED) # 确保 pkg-config 可用

    if (HX_NET_FIND_LIBURING)
        # 使用 pkg-config 检测 liburing
        pkg_check_modules(LIBURING liburing)
        if (NOT LIBURING_FOUND)
            # 如果 pkg-config 失败, 手动查找
            find_path(LIBURING_INCLUDE_DIR NAMES liburing.h)
            find_library(LIBURING_LIBRARY NAMES liburing liburing.a)
            if (NOT LIBURING_INCLUDE_DIR OR NOT LIBURING_LIBRARY)
                message(FATAL_ERROR "liburing not found")
            endif()
            set(LIBURING_LIBRARIES "${LIBURING_LIBRARY}")
            set(LIBURING_INCLUDE_DIRS "${LIBURING_INCLUDE_DIR}")
        endif()
        
        # 链接系统 liburing
        target_link_libraries(HXLibs PUBLIC "${LIBURING_LIBRARIES}")
        target_include_directories(HXLibs PUBLIC "${LIBURING_INCLUDE_DIRS}")
    else()
        # 使用项目内置的 liburing
        add_subdirectory(lib/liburing)                 # 先添加 liburing 库
        target_link_libraries(HXLibs PUBLIC liburing) # 链接 liburing 静态库
    endif()
endif()
