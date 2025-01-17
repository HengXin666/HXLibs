option(WEBSOCKET_SERVER "Build websocket-server" ON)

if (WEBSOCKET_SERVER)
    # 定义目标
    add_executable(websocket-server WsServer.cpp)

    # 链接 HXLibs 库
    target_link_libraries(websocket-server PUBLIC HXLibs)
endif()