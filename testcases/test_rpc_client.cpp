//
// Created by baitianyu on 7/25/24.
//
#include <assert.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <memory>
#include <unistd.h>
#include "common/log.h"
#include "common/config.h"
#include "common/log.h"
#include "net/tcp/net_addr.h"
#include "net/tcp/tcp_client.h"
#include "net/coder/string_coder.h"
#include "net/coder/abstract_protocol.h"
#include "net/coder/tinypb_protocol.h"
#include "net/coder/tinypb_coder.h"
#include <google/protobuf/service.h>
#include "order.pb.h"
#include "net/rpc/rpc_channel.h"
#include "net/rpc/rpc_controller.h"
#include "net/rpc/rpc_closure.h"

void test_rpc_channel() {
    rocket::IPNetAddr::net_addr_sptr_t_ addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 22224);
    auto channel = std::make_shared<rocket::RPCChannel>(addr);
    auto request = std::make_shared<makeOrderRequest>();
    request->set_price(100);
    request->set_goods("apple");
    auto response = std::make_shared<makeOrderResponse>();
    auto controller = std::make_shared<rocket::RPCController>();
    controller->SetMsgId("99998888");
    auto closure = std::make_shared<rocket::RPCClosure>([request, response, channel]() {
        INFOLOG("client call rpc success, request [%s], response [%s]", request->ShortDebugString().c_str(),
                response->ShortDebugString().c_str());
        INFOLOG("now exit client event loop");
        channel->GetClient()->stop();
    });
    channel->Init(controller, request, response, closure);
    Order_Stub stub(channel.get());
    stub.makeOrder(controller.get(), request.get(), response.get(), closure.get());
}

void test_rpc_client() {
    rocket::IPNetAddr::net_addr_sptr_t_ addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1", 22224);
    rocket::TCPClient client(addr);
    client.connect([addr, &client]() {
        DEBUGLOG("connect to [%s] success", addr->toString().c_str());
        std::shared_ptr<rocket::TinyPBProtocol> message = std::make_shared<rocket::TinyPBProtocol>();
        message->m_msg_id = "99998888";
        makeOrderRequest request;
        request.set_price(100);
        request.set_goods("apple");
        if (!request.SerializeToString(&(message->m_pb_data))) {
            ERRORLOG("serilize error");
            return;
        }
        message->m_method_name = "Order.makeOrder";
        client.writeMessage(message, [request](rocket::AbstractProtocol::abstract_pro_sptr_t_ msg_ptr) {
            DEBUGLOG("send message success, request[%s]", request.ShortDebugString().c_str());
        });
        client.readMessage("99998888", [](rocket::AbstractProtocol::abstract_pro_sptr_t_ msg_ptr) {
            std::shared_ptr<rocket::TinyPBProtocol> message = std::dynamic_pointer_cast<rocket::TinyPBProtocol>(
                    msg_ptr);
            DEBUGLOG("msg_id[%s], get response %s", message->m_msg_id.c_str(), message->m_pb_data.c_str());
            makeOrderResponse response;
            if (!response.ParseFromString(message->m_pb_data)) {
                ERRORLOG("deserialize error");
                return;
            }
            DEBUGLOG("get response success, response[%s]", response.ShortDebugString().c_str());
        });
    });
}

int main() {
    rocket::Config::SetGlobalConfig("../conf/rocket.xml");
    rocket::Logger::InitGlobalLogger();
    // test_rpc_client();
    test_rpc_channel();
}

