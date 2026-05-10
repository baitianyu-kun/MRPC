//
// Created by baitianyu on 2/11/25.
//
#include <unistd.h>
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>
#include "rpc/rpc_server.h"
#include "rpc/rpc_channel.h"
#include "order.pb.h"
#include "common/coroutine_task.h"

using namespace mrpc;

void initConfig() {
    Config::SetGlobalConfig("../../conf/mrpc.xml");
    Logger::InitGlobalLogger(0);
}

mrpc::Task<void> runTests(RPCChannel::ptr channel, std::shared_ptr<makeOrderRequest> request_msg, Order_Stub* stub) {
    auto call_method = std::bind(&Order_Stub::makeOrder, stub, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4);

    // Call 1
    auto response_msg = co_await channel->call<makeOrderRequest, makeOrderResponse>(call_method, request_msg);
    if (response_msg && response_msg->order_id() == "20230514") {
        INFOLOG("========= Success Call RPC By Coroutine 1 ==============");
    }

    // Call 2
    auto response_msg2 = co_await channel->call<makeOrderRequest, makeOrderResponse>(call_method, request_msg);
    if (response_msg2 && response_msg2->order_id() == "20230514") {
        INFOLOG("========= Success Call RPC By Coroutine 2 ==============");
    }

    // Call 3
    auto response_msg3 = co_await channel->call<makeOrderRequest, makeOrderResponse>(call_method, request_msg);
    if (response_msg3 && response_msg3->order_id() == "20230514") {
        INFOLOG("========= Success Call RPC By Coroutine 3 ==============");
    }

    // Call 4
    auto response_msg4 = co_await channel->call<makeOrderRequest, makeOrderResponse>(call_method, request_msg);
    if (response_msg4 && response_msg4->order_id() == "20230514") {
        INFOLOG("========= Success Call RPC By Coroutine 4 ==============");
    }
}

int main() {
    initConfig();

    auto client = std::make_shared<RPCClient>();

    auto runner = [&]() -> mrpc::Task<void> {
        co_await client->serviceDiscovery("Order");
    };
    auto task = runner();
    task.resume();

    while (client->getCacheSize() == 0) {} // 阻塞等待客户端进行服务发现

    auto channel = std::make_shared<RPCChannel>(client, client->getProtocolType());

    auto request_msg = std::make_shared<makeOrderRequest>();
    request_msg->set_price(100);
    request_msg->set_goods("apple");

    Order_Stub stub(channel.get());

    auto test_task = runTests(channel, request_msg, &stub);
    test_task.resume();
    
    // allow time for RPC to finish
    sleep(2);

    return 0;
}