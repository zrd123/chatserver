cmake_minimum_required(VERSION 3.0)
project(chat)

#配置编译选项
set(CMAKE_CXX_FLAGS   ${CMAKE_CXX_FLAGS} -g)

#配置最终可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

#配置头文件的搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/server)
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)
include_directories(${PROJECT_SOURCE_DIR}/include/server/model)
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis)
include_directories(${PROJECT_SOURCE_DIR}/include/server/protobuf)
include_directories(${PROJECT_SOURCE_DIR}/include/server/encryption)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)

#配置库文件的搜索路径
link_directories("/usr/lib64/mysql")
link_libraries("/usr/local/lib64/libabsl_log_internal_check_op.so.2301.0.0")
#加载子目录
add_subdirectory(src)
# set( CMAKE_BUILD_TYPE Release)
