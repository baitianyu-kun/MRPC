//
// Created by baitianyu on 2/11/25.
//
#include <unistd.h>
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>
#include "rpc/rpc_server.h"
#include "rpc/rpc_channel.h"
#include "rpc/rpc_controller.h"
#include "rpc/rpc_closure.h"
#include "order.pb.h"

using namespace mrpc;

int main() {
    Config::SetGlobalConfig("../conf/mrpc.xml");
    Logger::InitGlobalLogger(0);

    auto addr = std::make_shared<mrpc::IPNetAddr>(Config::GetGlobalConfig()->m_channel_peer_register_ip,
                                                  Config::GetGlobalConfig()->m_channel_peer_register_port);
    auto channel = std::make_shared<RPCChannel>(addr);

    auto request_msg = std::make_shared<makeOrderRequest>();
    request_msg->set_price(100);
    request_msg->set_goods("apple");

    auto response_msg = std::make_shared<makeOrderResponse>();
    auto controller = std::make_shared<RPCController>();

    auto closure = std::make_shared<RPCClosure>([request_msg, response_msg, channel, controller]() mutable {
        if (controller->GetErrorCode() == 0) {
            INFOLOG("call rpc success, request [%s], response [%s]",
                    request_msg->ShortDebugString().c_str(),
                    response_msg->ShortDebugString().c_str());
            // 执行业务逻辑
            if (response_msg->order_id() == "20230514") {
                INFOLOG("========= Success Call RPC ==============");
            }
        } else {
            ERRORLOG("call rpc failed, request [%s], error code [%d], error info [%s]",
                     response_msg->ShortDebugString().c_str(),
                     controller->GetErrorCode(),
                     controller->GetErrorInfo().c_str());
        }
        INFOLOG("now exit client event loop");
    });

    controller->SetTimeout(2000); // 设置超时时间
    channel->init(controller, request_msg, response_msg, closure);

    channel->subscribe("Order"); // 向注册中心订阅这个服务

    Order_Stub stub(channel.get());
    while (1) {
        stub.makeOrder(controller.get(), request_msg.get(), response_msg.get(), closure.get());
        usleep(100000); // 休眠3秒
    }
}