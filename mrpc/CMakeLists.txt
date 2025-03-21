cmake_minimum_required(VERSION 3.16)

# 读取所有源文件并放到SRC_LIST中，但是不会对子目录进行递归调用
aux_source_directory(src/common SRC_COMMON_LIST)
aux_source_directory(src/event SRC_EVENT_LIST)
aux_source_directory(src/rpc SRC_RPC_LIST)
aux_source_directory(src/net SRC_NET_LIST)
aux_source_directory(src/net/protocol SRC_NET_PROTOCOL_LIST)
aux_source_directory(src/net/protocol/http SRC_NET_HTTP_LIST)
aux_source_directory(src/net/protocol/mpb SRC_NET_MPB_LIST)
aux_source_directory(src/net/tcp SRC_NET_TCP_LIST)
aux_source_directory(src/net/balance SRC_NET_BALANCE_LIST)

# 将mrpc编译为动态链接库libmrpc.so，如果要编译为静态链接库则把SHARED更换为STATIC
add_library(mrpc SHARED ${SRC_COMMON_LIST}
        ${SRC_EVENT_LIST} ${SRC_RPC_LIST}
        ${SRC_NET_LIST} ${SRC_NET_PROTOCOL_LIST}
        ${SRC_NET_HTTP_LIST} ${SRC_NET_MPB_LIST}
        ${SRC_NET_TCP_LIST} ${SRC_NET_BALANCE_LIST})

# 头文件搜索
# include_directories(include)
# 不要使用include_directories(include)，这个会给下面的所有target都赋予头文件目录，容易污染头文件空间，应该分开给每个target设置头文件目录
target_include_directories(mrpc PUBLIC include)