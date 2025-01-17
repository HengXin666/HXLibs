option(FILE_SERVER "Build file-server" ON)

if (FILE_SERVER)
    # 定义目标
    add_executable(file-server FileServer.cpp)

    # 链接 HXLibs 库
    target_link_libraries(file-server PUBLIC HXLibs)
endif()