option(CLIENT "Build Client" ON)

if (CLIENT)
    # 定义目标
    add_executable(Client Client.cpp)

    # 链接 HXLibs 库
    target_link_libraries(Client PUBLIC HXLibs)
endif()