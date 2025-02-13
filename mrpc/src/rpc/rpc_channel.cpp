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

namespace mrpc {

    // subscribe方法就发送一个请求，用这个请求的地址当作服务端进行监听服务器的publish
    RPCChannel::RPCChannel(NetAddr::ptr register_center_addr) : m_register_center_addr(register_center_addr) {

    }

    RPCChannel::~RPCChannel() {
        DEBUGLOG("~RPCChannel");
    }

    void RPCChannel::init(RPCChannel::google_rpc_controller_ptr controller, RPCChannel::google_message_ptr request,
                          RPCChannel::google_message_ptr response, RPCChannel::google_closure_ptr done) {
        if (m_is_init) {
            return;
        }
        m_controller = controller;
        m_request = request;
        m_response = response;
        m_closure = done;
        m_is_init = true;
    }

    void RPCChannel::updateCache(const std::string &service_name, std::string &server_list) {
        std::vector<std::string> server_list_vec;
        splitStrToVector(server_list, ",", server_list_vec);
        std::set<NetAddr::ptr, CompNetAddr> server_list_set;
        auto find = m_service_servers_cache.find(service_name);
        if (find == m_service_servers_cache.end()) {
            auto con_hash = std::make_shared<ConsistentHash>();
            m_service_balance.emplace(service_name, con_hash);
        }
        for (const auto &server: server_list_vec) {
            server_list_set.emplace(std::make_shared<IPNetAddr>(server));
            // 插入到负载均衡中
            m_service_balance[service_name]->addNewPhysicalNode(server, VIRTUAL_NODE_NUM);
        }
        m_service_servers_cache.emplace(service_name, server_list_set);
    }

    void RPCChannel::subscribe(const std::string &service_name) {
        auto request = std::make_shared<HTTPRequest>();
        HTTPManager::body_type body;
        body["service_name"] = service_name;
        HTTPManager::createRequest(request, HTTPManager::MSGType::RPC_CLIENT_REGISTER_SUBSCRIBE_REQUEST, body);
        auto io_thread = std::make_unique<IOThread>();
        auto register_client = std::make_shared<TCPClient>(m_register_center_addr, io_thread->getEventLoop());
        register_client->connect([register_client, request, service_name]() {
            register_client->sendRequest(request, [](HTTPRequest::ptr req) {});
            register_client->recvResponse(request->m_msg_id,
                                          [register_client, request, service_name](HTTPResponse::ptr rsp) {
                                              register_client->getEventLoop()->stop();
                                              if (rsp->m_response_body_data_map["subscribe_success"] ==
                                                  std::to_string(true)) {
                                                  INFOLOG("%s | success subscribe service name %s",
                                                          rsp->m_msg_id.c_str(), service_name.c_str());
                                              }
                                          });
        });
        io_thread->start();
        // start listener at register client addr, call back is handlePublish，在handlePublish中重新执行从注册中心拉取操作
        // 实现推拉结合，在这里会进行阻塞，所以需要再次启动一个线程来进行

        // 后台线程中跑一个tcp server，然后这个后台线程tcp server中会创建线程中的线程池
        tmp_register_client_addr = register_client->getLocalAddr();

        pthread_t m_thread{0}; // 线程句柄，用来保存和操作线程的
        pthread_create(&m_thread, nullptr, RPCChannel::runner, this);
    }

    void *RPCChannel::runner(void *arg) {
        auto channel = reinterpret_cast<RPCChannel *>(arg);
        auto listener = std::make_shared<PublishListener>(channel->getTmpAddr(),
                                                          std::bind(&RPCChannel::handlePublish, channel,
                                                                    std::placeholders::_1,
                                                                    std::placeholders::_2,
                                                                    std::placeholders::_3));
        return nullptr;
    }

    void RPCChannel::handlePublish(HTTPRequest::ptr request, HTTPResponse::ptr response, HTTPSession::ptr session) {
        HTTPManager::body_type body;
        body["msg_id"] = request->m_msg_id;
        HTTPManager::createResponse(response, HTTPManager::MSGType::RPC_REGISTER_CLIENT_PUBLISH_RESPONSE, body);
        INFOLOG("success get publish message from register center, register center addr: [%s], local addr: [%s] request body: [%s]",
                session->getPeerAddr()->toString().c_str(),
                session->getLocalAddr()->toString().c_str(),
                request->m_request_body.c_str());
    }

    void RPCChannel::serviceDiscovery(const std::string &service_name) {
        auto io_thread = std::make_unique<IOThread>();
        auto register_client = std::make_shared<TCPClient>(m_register_center_addr, io_thread->getEventLoop());
        HTTPManager::body_type body;
        body["service_name"] = service_name;
        auto request = std::make_shared<HTTPRequest>();
        auto channel = shared_from_this();
        HTTPManager::createRequest(request, HTTPManager::MSGType::RPC_CLIENT_REGISTER_DISCOVERY_REQUEST, body);
        register_client->connect([register_client, request, channel, service_name]() {
            register_client->sendRequest(request, [](HTTPRequest::ptr req) {});
            register_client->recvResponse(request->m_msg_id,
                                          [register_client, request, channel, service_name](HTTPResponse::ptr rsp) {
                                              register_client->getEventLoop()->stop();
                                              // 更新本地缓存
                                              auto server_list_str = rsp->m_response_body_data_map["server_list"];
                                              channel->updateCache(service_name, server_list_str);
                                              INFOLOG("%s | get server cache from register center, server list %s",
                                                      rsp->m_msg_id.c_str(), channel->getAllServerList().c_str());
                                          });
        });
        io_thread->start();
    }

    std::string RPCChannel::getAllServerList() {
        std::string tmp = "";
        for (const auto &item: m_service_servers_cache) {
            tmp += "{" + item.first + ":";
            for (const auto &item: item.second) {
                tmp += item->toString() + ",";
            }
            tmp = tmp.substr(0, tmp.size() - 1);
            tmp += "}";
        }
        return tmp;
    }

    void RPCChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                google::protobuf::RpcController *controller, const google::protobuf::Message *request,
                                google::protobuf::Message *response, google::protobuf::Closure *done) {
        // 为空即去执行服务发现
        if (m_service_servers_cache.empty()) {
            serviceDiscovery(method->service()->full_name());
        }
        // 获取本地ip地址，根据该ip地址去进行hash选择
        auto local_ip = getLocalIP();
        auto server_addr = m_service_balance[method->service()->full_name()]->getServer(local_ip);
        DEBUGLOG("local ip [%s], choosing server [%s]", local_ip.c_str(), server_addr->toString().c_str());
        auto client = std::make_shared<TCPClient>(server_addr);

        auto request_protocol = std::make_shared<HTTPRequest>();
        auto rpc_controller = dynamic_cast<RPCController *>(controller);
        if (rpc_controller == nullptr) {
            ERRORLOG("failed call method, RpcController convert error");
            return;
        }
        // Order.makeOrder
        auto method_full_name = method->full_name();
        std::string req_pb_data;
        request->SerializeToString(&req_pb_data);

        HTTPManager::body_type body;
        body["method_full_name"] = method_full_name;
        body["pb_data"] = req_pb_data;
        HTTPManager::createRequest(request_protocol, HTTPManager::MSGType::RPC_METHOD_REQUEST, body);
        rpc_controller->SetMsgId(request_protocol->m_msg_id);
        INFOLOG("%s | call method name [%s]", request_protocol->m_msg_id.c_str(), method_full_name.c_str());

        auto this_channel = shared_from_this();
        client->connect([this_channel, request_protocol, client]() {
            // 发送请求
            client->sendRequest(request_protocol, [](HTTPRequest::ptr req) {});
            // 接收响应
            client->recvResponse(request_protocol->m_msg_id,
                                 [this_channel, request_protocol, client](HTTPResponse::ptr rsp) {
                                     this_channel->getResponse()->ParseFromString(
                                             rsp->m_response_body_data_map["pb_data"]);
                                     INFOLOG("%s | success get rpc response, peer addr [%s], local addr[%s], response [%s]",
                                             rsp->m_msg_id.c_str(),
                                             client->getPeerAddr()->toString().c_str(),
                                             client->getLocalAddr()->toString().c_str(),
                                             this_channel->getResponse()->ShortDebugString().c_str());
                                     if (this_channel->getClosure()) {
                                         this_channel->getClosure()->Run();
                                     }
                                     client->getEventLoop()->stop();
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
