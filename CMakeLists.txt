cmake_minimum_required(VERSION 3.16)

project(RPCFrame VERSION 1.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug") # 设置为debug即-g编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--no-as-needed -ldl")

# 总项目需要把个个子项目添加进来，具体逻辑在各个子项目中写出
add_subdirectory(mrpc)
add_subdirectory(testcases)

