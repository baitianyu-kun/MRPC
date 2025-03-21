cmake_minimum_required(VERSION 3.16)

# protobuf静态库
set(PROTOBUF_LIB /usr/local/lib/libprotobuf.a)
# 或link_directories(/usr/local/lib) + set(PROTOBUF_LIB libprotobuf.a)，前者指定链接库的目录，后者指定链接哪个文件


# 总lib
set(ALL_LIBS mrpc ${PROTOBUF_LIB} pthread)

add_executable(test_rpc_server
        test_rpc_server.cpp
        order.pb.cc
        order.pb.h)
# 链接静态库和动态库都是通过target_link_libraries来进行的，优先链接动态链接库，然后再找静态链接库
target_link_libraries(test_rpc_server ${ALL_LIBS})

# 链接动态库：
# link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../MyLib)
# add_executable(${PROJECT_NAME}
#               ${ALL_SOURCES}
#               ${ALL_INCLUDES})
# target_link_libraries(${PROJECT_NAME}
#    mrpc
#    # -lmrpc #前面也可以加-l)
# 或
# find_library(mrpcLib mrpc HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../MyLib)
# add_executable(${PROJECT_NAME}
#               ${ALL_SOURCES}
#               ${ALL_INCLUDES})
#target_link_libraries(${PROJECT_NAME}
#                       ${mrpcLib})

# 链接静态库(基本和动态库写法相同)：
# link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLib)
# add_executable(${PROJECT_NAME}
#               ${ALL_SOURCES}
#               ${ALL_INCLUDES})
# target_link_libraries(${PROJECT_NAME}
#    mrpc
#    # -lmrpc #前面也可以加-l
#    # libmrpc.a #可以明确指定静态库)
# 或
# find_library(mrpcLib mrpc HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdPartyLib)
# add_executable(${PROJECT_NAME}
#               ${ALL_SOURCES}
#               ${ALL_INCLUDES})
#target_link_libraries(${PROJECT_NAME}
#                       ${mrpcLib})

add_executable(test_rpc_server2
        test_rpc_server2.cpp
        order.pb.cc
        order.pb.h)
target_link_libraries(test_rpc_server2 ${ALL_LIBS})

add_executable(test_rpc_server3
        test_rpc_server3.cpp
        order.pb.cc
        order.pb.h)
target_link_libraries(test_rpc_server3 ${ALL_LIBS})

add_executable(test_rpc_channel_calling_mode
        test_rpc_channel_calling_mode.cpp
        order.pb.cc
        order.pb.h)
target_link_libraries(test_rpc_channel_calling_mode ${ALL_LIBS})

add_executable(test_rpc_channel_loop
        test_rpc_channel_loop.cpp
        order.pb.cc
        order.pb.h)
target_link_libraries(test_rpc_channel_loop ${ALL_LIBS})

add_executable(test_rpc_register_center_subscribe_publish
        test_rpc_register_center_subscribe_publish.cpp
        order.pb.cc
        order.pb.h)
target_link_libraries(test_rpc_register_center_subscribe_publish ${ALL_LIBS})

add_executable(test_rpc_server_unix
        test_rpc_server_unix.cpp
        order.pb.cc
        order.pb.h)
target_link_libraries(test_rpc_server_unix ${ALL_LIBS})

add_executable(test_rpc_channel_loop_unix
        test_rpc_channel_loop_unix.cpp
        order.pb.cc
        order.pb.h)
target_link_libraries(test_rpc_channel_loop_unix ${ALL_LIBS})