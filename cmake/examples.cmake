option(EXAMPLES "Build examples" ON)

if (EXAMPLES)
    # 查找 examples 目录下的所有 .cpp 文件
    file(GLOB_RECURSE EXAMPLE_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
    )

    foreach(EXAMPLE_FILE ${EXAMPLE_FILES})
        # 相对路径 (相对于 examples/)
        file(RELATIVE_PATH REL_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/examples ${EXAMPLE_FILE})

        # 目标名: examples/foo/bar.cpp -> foo_bar
        string(REPLACE "/" "_" EXAMPLE_NAME ${REL_EXAMPLE_PATH})
        string(REPLACE ".cpp" "" EXAMPLE_NAME ${EXAMPLE_NAME})

        # 添加目标
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE})
        target_link_libraries(${EXAMPLE_NAME} PRIVATE HXLibs)

        # 输出路径
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/examples/${PARENT_DIR}
        )

        # 提取父目录, 用于 FOLDER 分组
        get_filename_component(PARENT_DIR ${REL_EXAMPLE_PATH} DIRECTORY)

        # 设置 FOLDER（Visual Studio 工程结构）
        set_target_properties(${EXAMPLE_NAME} PROPERTIES FOLDER examples/${PARENT_DIR})

        # 在 IDE 中保留原始目录结构
        source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/examples FILES ${EXAMPLE_FILE})

        # Address Sanitizer 支持
        if(HX_DEBUG_BY_ADDRESS_SANITIZER)
            target_compile_options(${EXAMPLE_NAME} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address>)
            target_link_options(${EXAMPLE_NAME} PRIVATE
                $<$<CONFIG:Debug>:-fsanitize=address>)
        endif()
    endforeach()
endif()
