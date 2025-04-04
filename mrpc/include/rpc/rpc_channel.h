//
// Created by baitianyu on 2/11/25.
//

#ifndef RPCFRAME_RPC_CHANNEL_H
#define RPCFRAME_RPC_CHANNEL_H

#include <google/protobuf/service.h>
#include <future>
#include "net/tcp/net_addr.h"
#include "net/tcp/tcp_client.h"
#include "rpc/rpc_controller.h"
#include "rpc/rpc_closure.h"
#include "common/log.h"
#include "rpc/rpc_client.h"


#define NEW_MESSAGE(type, var_name) \
        std::shared_ptr<type> var_name = std::make_shared<type>(); \

#define NEW_RPC_CONTROLLER(var_name) \
        std::shared_ptr<mrpc::RPCController> var_name = std::make_shared<mrpc::RPCController>(); \

#define NEW_RPC_CHANNEL(addr, var_name) \
        std::shared_ptr<mrpc::RPCChannel> var_name = std::make_shared<mrpc::RPCChannel>(addr); \

#define CALL_RPC(addr, stub_name, method_name, controller, request, response, closure) \
        {                                                                              \
            channel->init(controller, request, response, closure); \
            stub_name(channel.get()).method_name(controller.get(), request.get(), response.get(), closure.get()); \
        }

// channel连接注册中心进行discovery，注册中心收到后记录下channel的地址，然后向channel推送消息
// channel是tcpclient，也是tcpserver，用来接收注册中心推送的消息
namespace mrpc {

    class RPCChannel : public google::protobuf::RpcChannel, public std::enable_shared_from_this<RPCChannel> {
    public:
        using ptr = std::shared_ptr<RPCChannel>;
        using google_rpc_controller_ptr = std::shared_ptr<google::protobuf::RpcController>;
        using google_message_ptr = std::shared_ptr<google::protobuf::Message>;
        using google_closure_ptr = std::shared_ptr<google::protobuf::Closure>;

        explicit RPCChannel(RPCClient::ptr rpc_client, ProtocolType protocol_type = ProtocolType::HTTP_Protocol);

        ~RPCChannel() override;

        void init(google_rpc_controller_ptr controller, google_message_ptr request,
                  google_message_ptr response, google_closure_ptr done);

        void CallMethod(const google::protobuf::MethodDescriptor *method,
                        google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                        google::protobuf::Message *response, google::protobuf::Closure *done) override;

        template<typename RequestMsgType, typename ResponseMsgType>
        void callRPCAsync(
                std::function<void(RPCController *, RequestMsgType *, ResponseMsgType *, RPCClosure *)> call_method,
                std::shared_ptr<RequestMsgType> request_msg,
                std::function<void(std::shared_ptr<ResponseMsgType>)> response_msg_call_back) {
            bool done = false; // 等待异步操作完成
            callRPCAsyncIn<RequestMsgType, ResponseMsgType>(call_method, request_msg,
                                                            [&done, response_msg_call_back](
                                                                    std::shared_ptr<ResponseMsgType> response_msg) {
                                                                response_msg_call_back(response_msg);
                                                                done = true;
                                                            });
            while (!done) {}
        }

        template<typename RequestMsgType, typename ResponseMsgType>
        std::future<std::shared_ptr<ResponseMsgType>> callRPCFuture(
                std::function<void(RPCController *, RequestMsgType *, ResponseMsgType *, RPCClosure *)> call_method,
                std::shared_ptr<RequestMsgType> request_msg) {
            auto promise = std::make_shared<std::promise<std::shared_ptr<ResponseMsgType>>>();
            auto future = promise->get_future();
            callRPCAsyncIn<RequestMsgType, ResponseMsgType>(call_method, request_msg,
                                                            [promise](std::shared_ptr<ResponseMsgType> response_msg) {
                                                                if (response_msg) {
                                                                    promise->set_value(response_msg);
                                                                } else {
                                                                    promise->set_exception(std::make_exception_ptr(
                                                                            std::runtime_error("error response_msg")));
                                                                }
                                                            });
            return future;
        }

        google::protobuf::RpcController *getController();

        google::protobuf::Message *getRequest();

        google::protobuf::Message *getResponse();

        google::protobuf::Closure *getClosure();

    private:
        template<typename RequestMsgType, typename ResponseMsgType>
        void callRPCAsyncIn(
                std::function<void(RPCController *, RequestMsgType *, ResponseMsgType *, RPCClosure *)> call_method,
                std::shared_ptr<RequestMsgType> request_msg,
                std::function<void(std::shared_ptr<ResponseMsgType>)> response_msg_call_back) {
            auto response_msg = std::make_shared<ResponseMsgType>();
            auto controller = std::make_shared<RPCController>();
            auto closure = std::make_shared<RPCClosure>(
                    [request_msg, response_msg, controller, response_msg_call_back]() mutable {
                        if (controller->GetErrorCode() == 0) {
                            INFOLOG("call rpc success, request [%s], response [%s]",
                                    request_msg->ShortDebugString().c_str(),
                                    response_msg->ShortDebugString().c_str());
                            if (response_msg_call_back) {
                                response_msg_call_back(response_msg);
                            }
                        } else {
                            ERRORLOG("call rpc failed, request [%s], error code [%d], error info [%s]",
                                     response_msg->ShortDebugString().c_str(),
                                     controller->GetErrorCode(),
                                     controller->GetErrorInfo().c_str());
                            if (response_msg_call_back) {
                                response_msg_call_back(nullptr);
                            }
                        }
                    });
            controller->SetTimeout(2000); // 设置超时时间
            init(controller, request_msg, response_msg, closure);
            call_method(controller.get(), request_msg.get(), response_msg.get(), closure.get());
        }

    private:
        google_rpc_controller_ptr m_controller{nullptr};
        google_message_ptr m_request{nullptr};
        google_message_ptr m_response{nullptr};
        google_closure_ptr m_closure{nullptr};
        ProtocolType m_protocol_type;
        RPCClient::ptr m_rpc_client;
    };
}

#endif //RPCFRAME_RPC_CHANNEL_H
