cmake_minimum_required(VERSION 3.10)

project(RPCFrame VERSION 1.0)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE "Debug") # 设置为debug即-g编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--no-as-needed -ldl")

# 设置文件输出路径
set(PATH_LIB ${PROJECT_SOURCE_DIR}/lib)
set(PATH_BIN ${PROJECT_SOURCE_DIR}/bin)
set(PATH_TESTCASES testcases)

# 头文件搜索
include_directories(${PROJECT_SOURCE_DIR}/mrpc/include)

# 读取所有源文件并放到SRC_LIST中，但是不会对子目录进行递归调用
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/common SRC_COMMON_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/event SRC_EVENT_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/rpc SRC_RPC_LIST)

aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/net SRC_NET_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/net/protocol SRC_NET_PROTOCOL_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/net/protocol/http SRC_NET_HTTP_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/net/protocol/mpb SRC_NET_MPB_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/net/tcp SRC_NET_TCP_LIST)
aux_source_directory(${PROJECT_SOURCE_DIR}/mrpc/src/net/balance SRC_NET_BALANCE_LIST)

# 本项目的lib
add_library(mrpc ${SRC_COMMON_LIST} ${SRC_EVENT_LIST} ${SRC_RPC_LIST} ${SRC_NET_LIST} ${SRC_NET_PROTOCOL_LIST}
        ${SRC_NET_HTTP_LIST} ${SRC_NET_MPB_LIST} ${SRC_NET_TCP_LIST} ${SRC_NET_BALANCE_LIST})
install(TARGETS mrpc DESTINATION ${PATH_LIB})

# 第三方lib
set(PROTOBUF_LIB /usr/local/lib/libprotobuf.a)

# 总lib
set(ALL_LIBS mrpc ${PROTOBUF_LIB} pthread)


add_executable(test_rpc_server
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_server.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_server ${ALL_LIBS})

add_executable(test_rpc_server2
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_server2.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_server2 ${ALL_LIBS})

add_executable(test_rpc_server3
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_server3.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_server3 ${ALL_LIBS})

add_executable(test_rpc_channel_calling_mode
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_channel_calling_mode.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_channel_calling_mode ${ALL_LIBS})

add_executable(test_rpc_channel_loop
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_channel_loop.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_channel_loop ${ALL_LIBS})

add_executable(test_rpc_register_center_subscribe_publish
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_register_center_subscribe_publish.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_register_center_subscribe_publish ${ALL_LIBS})

add_executable(test_rpc_server_unix
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_server_unix.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_server_unix ${ALL_LIBS})

add_executable(test_rpc_channel_loop_unix
        ${PROJECT_SOURCE_DIR}/${PATH_TESTCASES}/test_rpc_channel_loop_unix.cpp
        testcases/order.pb.cc
        testcases/order.pb.h)
target_link_libraries(test_rpc_channel_loop_unix ${ALL_LIBS})

