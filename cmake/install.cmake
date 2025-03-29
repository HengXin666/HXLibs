# 递归查找, 然后结果存储在`srcs`
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS 
    src/*.cpp
)

#   ##### 编译选项 ##### {

# 是否 启用 搜集客户端地址信息
option(CLIENT_ADDRESS_LOGGING OFF)

if (CLIENT_ADDRESS_LOGGING)
    # 启用 搜集客户端地址信息 宏
    add_definitions(-DCLIENT_ADDRESS_LOGGING)
endif()

# } ##### 编译选项 #####

add_library(HXLibs STATIC ${SRC_FILES})
add_library(HXLibs::HXLibs ALIAS HXLibs)

target_compile_features(HXLibs PUBLIC cxx_std_20)

# 导入第三方库
include(cmake/includeLib.cmake)

# 包含头文件目录
target_include_directories(HXLibs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  # 供当前构建使用
    $<INSTALL_INTERFACE:include>                            # 供安装后使用
)

# 安装目标
install(TARGETS HXLibs EXPORT HXLibsTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 创建导出文件
export(EXPORT HXLibsTargets FILE "${CMAKE_CURRENT_BINARY_DIR}/HXLibsTargets.cmake")

# 生成并安装 CMake 配置文件，供 `find_package(HXLibs)` 使用
install(EXPORT HXLibsTargets
    FILE HXLibsTargets.cmake
    NAMESPACE HXLibs::
    DESTINATION lib/cmake/HXLibs
)

# 生成版本信息文件
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HXLibsConfigVersion.cmake"
    VERSION ${HX_LIBS_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# 安装版本信息文件
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HXLibsConfigVersion.cmake"
    DESTINATION lib/cmake/HXLibs
)
