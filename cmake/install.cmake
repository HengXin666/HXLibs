# 头文件-only库
add_library(HXLibs INTERFACE)
add_library(HXLibs::HXLibs ALIAS HXLibs)

target_include_directories(HXLibs INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

option(CLIENT_ADDRESS_LOGGING OFF)

if (CLIENT_ADDRESS_LOGGING)
    target_compile_definitions(HXLibs INTERFACE CLIENT_ADDRESS_LOGGING)
endif()

# 安装头文件
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
)

# 导出 target
install(TARGETS HXLibs
    EXPORT HXLibsTargets
)

# 导出 CMake 配置
install(EXPORT HXLibsTargets
    FILE HXLibsTargets.cmake
    NAMESPACE HXLibs::
    DESTINATION lib/cmake/HXLibs
)

# 生成版本信息
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HXLibsConfigVersion.cmake"
    VERSION ${HX_LIBS_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/HXLibsConfigVersion.cmake"
    DESTINATION lib/cmake/HXLibs
)
