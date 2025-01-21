option(SERVER_ADD_INTERCEPTOR "Build server-add-interceptor" ON)

if (SERVER_ADD_INTERCEPTOR)
    # 定义目标
    add_executable(server-add-interceptor ServerAddInterceptor.cpp)

    # 链接 HXLibs 库
    target_link_libraries(server-add-interceptor PUBLIC HXLibs)
endif()