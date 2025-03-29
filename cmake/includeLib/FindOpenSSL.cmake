# 查找 OpenSSL 库 (如果找不到, CMake 会自动报错)
find_package(OpenSSL REQUIRED)

# 链接 OpenSSL 库
target_link_libraries(HXLibs PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# 可选: 如果需要明确添加 OpenSSL 的头文件路径
# 一般情况下, find_package 已经会自动添加头文件路径到目标中
# target_include_directories(HXLibs PUBLIC ${OPENSSL_INCLUDE_DIR})