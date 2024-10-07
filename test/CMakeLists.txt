cmake_minimum_required(VERSION 3.12)
project(MyProject)

# 指定 C++ 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE Debug)   # debug 模式

# 自动将 src 目录下的所有源文件添加到 SRC_LIST 变量
file(GLOB SRC_LIST "../**/*.cpp")

# 查找 pthread 库
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 查找 MySQL client 库
find_library(MYSQLCLIENT_LIBRARY mysqlclient)

# 创建可执行文件，使用自动添加的源文件
add_executable(MyProject ${SRC_LIST})

target_link_libraries(MyProject PRIVATE Threads::Threads ${MYSQLCLIENT_LIBRARY} rt)


