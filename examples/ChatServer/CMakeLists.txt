option(CHAT_SERVER "Build chat-server" ON)

if (CHAT_SERVER)
    # 定义目标
    add_executable(chat-server ChatServer.cpp)

    # 链接 HXLibs 库
    target_link_libraries(chat-server PUBLIC HXLibs)
endif()