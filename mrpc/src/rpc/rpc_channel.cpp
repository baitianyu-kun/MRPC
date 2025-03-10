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

    // subscribe方法就发送一个请求，用这个请求的地址当作服务端进行监听服务器的publish
    RPCChannel::RPCChannel() {
        m_register_center_addr = std::make_shared<mrpc::IPNetAddr>(
                Config::GetGlobalConfig()->m_channel_peer_register_ip,
                Config::GetGlobalConfig()->m_channel_peer_register_port);
        if (Config::GetGlobalConfig()->m_protocol == "MPB") {
            m_protocol_type = ProtocolType::MPb_Protocol;
        } else {
            m_protocol_type = ProtocolType::HTTP_Protocol;
        }
    }

    RPCChannel::~RPCChannel() {
        DEBUGLOG("~RPCChannel");
    }

    void RPCChannel::init(RPCChannel::google_rpc_controller_ptr controller, RPCChannel::google_message_ptr request,
                          RPCChannel::google_message_ptr response, RPCChannel::google_closure_ptr done) {
        m_controller = controller;
        m_request = request;
        m_response = response;
        m_closure = done;
    }

    void RPCChannel::updateCache(const std::string &service_name, std::string &server_list) {
        m_service_servers_cache.clear();
        m_service_balance.clear();

        std::vector<std::string> server_list_vec;
        splitStrToVector(server_list, ",", server_list_vec);
        std::set<std::string> server_list_set;
        auto find = m_service_servers_cache.find(service_name);
        if (find == m_service_servers_cache.end()) {
            auto con_hash = std::make_shared<ConsistentHash>();
            m_service_balance.emplace(service_name, con_hash);
        }
        for (const auto &server: server_list_vec) {
            server_list_set.emplace(server);
            // 插入到负载均衡中
            m_service_balance[service_name]->addNewPhysicalNode(server, VIRTUAL_NODE_NUM);
        }
        m_service_servers_cache.emplace(service_name, server_list_set);
    }

    void RPCChannel::subscribe(const std::string &service_name) {
        body_type body;
        body["service_name"] = service_name;
        Protocol::ptr request = nullptr;
        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            request = std::make_shared<HTTPRequest>();
            HTTPManager::createRequest(std::static_pointer_cast<HTTPRequest>(request),
                                       MSGType::RPC_CLIENT_REGISTER_SUBSCRIBE_REQUEST, body);
        } else {
            request = std::make_shared<MPbProtocol>();
            MPbManager::createRequest(std::static_pointer_cast<MPbProtocol>(request),
                                      MSGType::RPC_CLIENT_REGISTER_SUBSCRIBE_REQUEST, body);
        }
        auto io_thread = std::make_unique<IOThread>();
        auto register_client = std::make_shared<TCPClient>(m_register_center_addr,
                                                           io_thread->getEventLoop(),
                                                           m_protocol_type);
        register_client->connect([&register_client, request, service_name]() {
            register_client->sendRequest(request, [&register_client, request, service_name](Protocol::ptr req) {
                register_client->recvResponse(request->m_msg_id,
                                              [&register_client, request, service_name](Protocol::ptr rsp) {
                                                  register_client->getEventLoop()->stop();
                                                  if (rsp->m_body_data_map["subscribe_success"] ==
                                                      std::to_string(true)) {
                                                      INFOLOG("%s | success subscribe service name %s",
                                                              rsp->m_msg_id.c_str(), service_name.c_str());
                                                  }
                                              });
            });
        });
        io_thread->start();

        // start listener at register client addr, call back is handlePublish，在handlePublish中重新执行从注册中心拉取操作
        // 实现推拉结合，在这里会进行阻塞，所以需要再次启动一个线程来进行，启动线程已经在PublishListener中进行封装
        m_publish_listener = std::make_shared<PublishListener>(register_client->getLocalAddr(),
                                                               std::bind(&RPCChannel::handlePublish,
                                                                         this,
                                                                         std::placeholders::_1,
                                                                         std::placeholders::_2,
                                                                         std::placeholders::_3),
                                                               m_protocol_type);
    }

    void RPCChannel::handlePublish(Protocol::ptr request, Protocol::ptr response, Session::ptr session) {
        DEBUGLOG("update channel server cache before: [%s]", getAllServerList().c_str());
        auto service_name = request->m_body_data_map["service_name"];
        serviceDiscovery(service_name); // 收到服务器通知后重新请求服务信息，推拉结合
        DEBUGLOG("update channel server cache after: [%s]", getAllServerList().c_str());

        body_type body;
        body["msg_id"] = request->m_msg_id;
        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            HTTPManager::createResponse(std::static_pointer_cast<HTTPResponse>(response),
                                        MSGType::RPC_REGISTER_CLIENT_PUBLISH_RESPONSE, body);
        } else {
            MPbManager::createResponse(std::static_pointer_cast<MPbProtocol>(response),
                                       MSGType::RPC_REGISTER_CLIENT_PUBLISH_RESPONSE, body);
        }
        INFOLOG(" %s | success get publish message from register center, register center addr: [%s], local addr: [%s]",
                request->m_msg_id.c_str(),
                session->getPeerAddr()->toString().c_str(),
                session->getLocalAddr()->toString().c_str());
    }

    void RPCChannel::serviceDiscovery(const std::string &service_name) {
        auto io_thread = std::make_unique<IOThread>();
        auto register_client = std::make_shared<TCPClient>(m_register_center_addr, io_thread->getEventLoop(),
                                                           m_protocol_type);
        auto channel = shared_from_this();
        body_type body;
        body["service_name"] = service_name;
        Protocol::ptr request = nullptr;
        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            request = std::make_shared<HTTPRequest>();
            HTTPManager::createRequest(std::static_pointer_cast<HTTPRequest>(request),
                                       MSGType::RPC_CLIENT_REGISTER_DISCOVERY_REQUEST, body);
        } else {
            request = std::make_shared<MPbProtocol>();
            MPbManager::createRequest(std::static_pointer_cast<MPbProtocol>(request),
                                      MSGType::RPC_CLIENT_REGISTER_DISCOVERY_REQUEST, body);
        }
        register_client->connect([&register_client, request, channel, service_name]() {
            register_client->sendRequest(request,
                                         [&register_client, request, channel, service_name](Protocol::ptr req) {
                                             register_client->recvResponse(request->m_msg_id,
                                                                           [&register_client, request, channel, service_name](
                                                                                   Protocol::ptr rsp) {
                                                                               // 更新本地缓存
                                                                               auto server_list_str = rsp->m_body_data_map["server_list"];
                                                                               channel->updateCache(service_name,
                                                                                                    server_list_str);
                                                                               // 获取本地ip地址，根据该ip地址去进行hash选择
                                                                               auto local_ip = getLocalIP();
                                                                               auto server_addr = channel->m_service_balance[service_name]->getServer(
                                                                                       local_ip);
                                                                               DEBUGLOG(
                                                                                       "local ip [%s], choosing server [%s]",
                                                                                       local_ip.c_str(),
                                                                                       server_addr->toString().c_str());
                                                                               // 为该服务器创建连接池
                                                                               channel->m_tcp_client_pool = std::make_unique<TCPClientPool>(
                                                                                       server_addr,
                                                                                       EventLoop::GetCurrentEventLoop(),
                                                                                       channel->m_protocol_type
                                                                               );
                                                                               INFOLOG("%s | get server cache from register center, server list [%s]",
                                                                                       rsp->m_msg_id.c_str(),
                                                                                       channel->getAllServerList().c_str());
                                                                               register_client->getEventLoop()->stop();
                                                                           });
                                         });
        });
        io_thread->start();
    }

    std::string RPCChannel::getAllServerList() {
        std::string tmp = "";
        for (const auto &item: m_service_servers_cache) {
            tmp += "{" + item.first + ":";
            for (const auto &item: item.second) {
                tmp += item + ",";
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

        m_tcp_client_pool->getConnectionAsync([request_protocol, this_channel](TCPClient::ptr client) {
            client->sendRequest(request_protocol, [this_channel, request_protocol, client](Protocol::ptr req) {
                client->recvResponse(request_protocol->m_msg_id,
                                     [this_channel, request_protocol, client](Protocol::ptr rsp) {
                                         this_channel->getResponse()->ParseFromString(
                                                 rsp->m_body_data_map["pb_data"]);
                                         INFOLOG("%s | success get rpc response, peer addr [%s], local addr[%s], response [%s]",
                                                 rsp->m_msg_id.c_str(),
                                                 client->getPeerAddr()->toString().c_str(),
                                                 client->getLocalAddr()->toString().c_str(),
                                                 this_channel->getResponse()->ShortDebugString().c_str());
                                         if (this_channel->getClosure()) {
                                             this_channel->getClosure()->Run();
                                         }
                                         this_channel->m_tcp_client_pool->releaseClient(client);
                                         client->clear();
                                     });
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