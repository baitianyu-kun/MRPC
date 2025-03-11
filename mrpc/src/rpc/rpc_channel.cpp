//
// Created by baitianyu on 2/11/25.
//
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "rpc/rpc_channel.h"
#include "rpc/rpc_controller.h"
#include "common/log.h"
#include "common/string_util.h"
#include "event/io_thread.h"
#include "common/util.h"
#include "rpc/rpc_closure.h"

namespace mrpc {

    RPCChannel::RPCChannel(RPCClient::ptr rpc_client, ProtocolType protocol_type) :
            m_rpc_client(rpc_client),
            m_protocol_type(protocol_type) {

    }

    RPCChannel::~RPCChannel() {
        DEBUGLOG("~RPCChannel");
    }

    void RPCChannel::init(
            RPCChannel::google_rpc_controller_ptr controller,
            RPCChannel::google_message_ptr request,
            RPCChannel::google_message_ptr response,
            RPCChannel::google_closure_ptr done) {
        m_controller = controller;
        m_request = request;
        m_response = response;
        m_closure = done;
    }

    void RPCChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                                google::protobuf::Message *response, google::protobuf::Closure *done) {
        auto rpc_controller = dynamic_cast<RPCController *>(controller);
        if (rpc_controller == nullptr) {
            ERRORLOG("failed call method, RpcController convert error");
            return;
        }
        // Order.makeOrder
        auto method_full_name = method->full_name();
        std::string req_pb_data;
        request->SerializeToString(&req_pb_data);

        body_type body;
        body["method_full_name"] = method_full_name;
        body["pb_data"] = req_pb_data;
        Protocol::ptr request_protocol = nullptr;
        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            request_protocol = std::make_shared<HTTPRequest>();
            HTTPManager::createRequest(std::static_pointer_cast<HTTPRequest>(request_protocol),
                                       MSGType::RPC_METHOD_REQUEST, body);
        } else {
            request_protocol = std::make_shared<MPbProtocol>();
            MPbManager::createRequest(std::static_pointer_cast<MPbProtocol>(request_protocol),
                                      MSGType::RPC_METHOD_REQUEST, body);
        }
        rpc_controller->SetMsgId(request_protocol->m_msg_id);
        INFOLOG("%s | call method name [%s]", request_protocol->m_msg_id.c_str(), method_full_name.c_str());

        auto this_channel = shared_from_this();

        while (m_rpc_client->getClient()->getRunning()) {} // 一个连接上同时只能有一个请求
        m_rpc_client->getClient()->setRunning(true);
        m_rpc_client->getClient()->sendRequest(request_protocol, [this_channel, request_protocol](Protocol::ptr req) {
            this_channel->m_rpc_client->getClient()->recvResponse(request_protocol->m_msg_id,
                                                                  [this_channel, request_protocol](Protocol::ptr rsp) {
                                                                      this_channel->getResponse()->ParseFromString(
                                                                              rsp->m_body_data_map["pb_data"]);
                                                                      INFOLOG("%s | success get rpc response, peer addr [%s], local addr[%s], response [%s]",
                                                                              rsp->m_msg_id.c_str(),
                                                                              this_channel->m_rpc_client->getClient()->getPeerAddr()->toString().c_str(),
                                                                              this_channel->m_rpc_client->getClient()->getLocalAddr()->toString().c_str(),
                                                                              this_channel->getResponse()->ShortDebugString().c_str());
                                                                      if (this_channel->getClosure()) {
                                                                          this_channel->getClosure()->Run();
                                                                      }
                                                                      this_channel->m_rpc_client->getClient()->resetNew();
                                                                      this_channel->m_rpc_client->getClient()->setRunning(
                                                                              false);
                                                                  });
        });
    }

    google::protobuf::RpcController *RPCChannel::getController() {
        return m_controller.get();
    }

    google::protobuf::Message *RPCChannel::getRequest() {
        return m_request.get();
    }

    google::protobuf::Message *RPCChannel::getResponse() {
        return m_response.get();
    }

    google::protobuf::Closure *RPCChannel::getClosure() {
        return m_closure.get();
    }
}