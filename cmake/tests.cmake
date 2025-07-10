# 递归查找 tests/ 目录下的所有 .cpp 文件
file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

# 添加 include 路径
include_directories(tests/include)

foreach(TEST_FILE ${TEST_FILES})
    # 将 tests/ 之后的路径提取出来 (相对路径)
    file(RELATIVE_PATH REL_TEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tests ${TEST_FILE})

    # 提取父目录名用于 FOLDER
    get_filename_component(PARENT_DIR ${REL_TEST_PATH} DIRECTORY)

    # 提取文件名作为目标名
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    # 添加源文件对应的目标
    add_executable(${TEST_NAME} ${TEST_FILE})

    # 链接库
    target_link_libraries(${TEST_NAME} PRIVATE HXLibs)

    # 设置可执行文件输出目录（按目录分类可选）
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output/tests/${PARENT_DIR}
    )

    # 添加 ctest 测试项
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # 设置 FOLDER 属性 (适用于 Visual Studio 工程组织)
    set_target_properties(${TEST_NAME} PROPERTIES FOLDER tests/${PARENT_DIR})

    # 在 IDE 中显示原始结构
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/tests FILES ${TEST_FILE})

    # 启用 AddressSanitizer（仅 Debug 模式）
    if(HX_DEBUG_BY_ADDRESS_SANITIZER)
        target_compile_options(${TEST_NAME} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address>)
        target_link_options(${TEST_NAME} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address>)
    endif()
endforeach()
