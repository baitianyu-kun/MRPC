//
// Created by baitianyu on 2/11/25.
//
#include <unistd.h>
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>
#include "rpc/rpc_server.h"
#include "rpc/rpc_channel.h"
#include "order.pb.h"

using namespace mrpc;

void initConfig() {
    Config::SetGlobalConfig("../conf/mrpc.xml");
    Logger::InitGlobalLogger(0);
}


int main() {
    initConfig();

    auto client = std::make_shared<RPCClient>();

    client->serviceDiscovery("Order");

    while (client->getCacheSize() == 0) {} // 阻塞等待客户端进行服务发现

    auto channel = std::make_shared<RPCChannel>(client, client->getProtocolType());

    auto request_msg = std::make_shared<makeOrderRequest>();
    request_msg->set_price(100);
    request_msg->set_goods("apple");

    Order_Stub stub(channel.get());


    auto future = channel->callRPCFuture<makeOrderRequest, makeOrderResponse>(
            std::bind(&Order_Stub::makeOrder, &stub, std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3, std::placeholders::_4),
            request_msg);
    auto response_msg = future.get();
    if (response_msg->order_id() == "20230514") {
        INFOLOG("========= Success Call RPC By Future ==============");
    }

    auto future2 = channel->callRPCFuture<makeOrderRequest, makeOrderResponse>(
            std::bind(&Order_Stub::makeOrder, &stub, std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3, std::placeholders::_4),
            request_msg);
    auto response_msg2 = future2.get();
    if (response_msg2->order_id() == "20230514") {
        INFOLOG("========= Success Call RPC By Future ==============");
    }


    channel->callRPCAsync<makeOrderRequest, makeOrderResponse>(
            std::bind(&Order_Stub::makeOrder, &stub,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3, std::placeholders::_4),
            request_msg,
            [](std::shared_ptr<makeOrderResponse> response_msg) {
                if (response_msg->order_id() == "20230514") {
                    INFOLOG("========= Success Call RPC By Async ==============");
                }
            }
    );

    channel->callRPCAsync<makeOrderRequest, makeOrderResponse>(
            std::bind(&Order_Stub::makeOrder, &stub,
                      std::placeholders::_1, std::placeholders::_2,
                      std::placeholders::_3, std::placeholders::_4),
            request_msg,
            [](std::shared_ptr<makeOrderResponse> response_msg) {
                if (response_msg->order_id() == "20230514") {
                    INFOLOG("========= Success Call RPC By Async ==============");
                }
            }
    );
    return 0;
}