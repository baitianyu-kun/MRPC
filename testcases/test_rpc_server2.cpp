//
// Created by baitianyu on 2/11/25.
//
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>
#include "rpc/rpc_server.h"
#include "order.pb.h"

using namespace mrpc;

class OrderImpl : public Order {
public:
    OrderImpl() = default;

    ~OrderImpl() override = default;

    void makeOrder(google::protobuf::RpcController *controller,
                   const ::makeOrderRequest *request,
                   ::makeOrderResponse *response,
                   ::google::protobuf::Closure *done) override {
        if (request->price() < 10) {
            response->set_ret_code(-1);
            response->set_res_info("short balance");
            return;
        }
        response->set_order_id("20230514");
        if (done) {
            done->Run();
            delete done;
            done = nullptr;
        }
    }
};

int main() {
    Config::SetGlobalConfig("../../conf/mrpc2.xml");
    Logger::InitGlobalLogger(0);

    auto local_addr = std::make_shared<mrpc::IPNetAddr>(Config::GetGlobalConfig()->m_rpc_server_listen_ip,
                                                        Config::GetGlobalConfig()->m_rpc_server_listen_port);
    auto register_addr = std::make_shared<mrpc::IPNetAddr>(Config::GetGlobalConfig()->m_server_peer_register_ip,
                                                           Config::GetGlobalConfig()->m_server_peer_register_port);
    ProtocolType protocol;
    if (Config::GetGlobalConfig()->m_protocol=="MPB"){
        protocol = ProtocolType::MPb_Protocol;
    }else{
        protocol = ProtocolType::HTTP_Protocol;
    }
    auto rpc_server = std::make_unique<RPCServer>(local_addr, register_addr,protocol);

    auto service = std::make_shared<OrderImpl>();
    rpc_server->addService(service);

    rpc_server->startRPC();
    return 0;
}