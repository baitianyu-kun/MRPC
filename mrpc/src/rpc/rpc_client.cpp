//
// Created by baitianyu on 25-3-11.
//
#include "rpc/rpc_client.h"
#include "common/util.h"
#include "common/string_util.h"

namespace mrpc {

    RPCClient::RPCClient() {
        m_register_center_addr = std::make_shared<mrpc::IPNetAddr>(
                Config::GetGlobalConfig()->m_channel_peer_register_ip,
                Config::GetGlobalConfig()->m_channel_peer_register_port);
        if (Config::GetGlobalConfig()->m_protocol == "MPB") {
            m_protocol_type = ProtocolType::MPb_Protocol;
        } else {
            m_protocol_type = ProtocolType::HTTP_Protocol;
        }
        m_call_io_thread = std::make_unique<IOThread>();
        m_call_io_thread->start();
    }

    RPCClient::~RPCClient() {
        DEBUGLOG("~RPCClient")
    }

    void RPCClient::serviceDiscovery(const std::string &service_name) {
        auto io_thread = std::make_unique<IOThread>();
        auto register_client = std::make_shared<TCPClient>(m_register_center_addr, io_thread->getEventLoop(),
                                                           m_protocol_type);
        auto rpc_client = shared_from_this();
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
        register_client->connect([&register_client, request, rpc_client, service_name]() {
            register_client->sendRequest(request,
                                         [&register_client, request, rpc_client, service_name](Protocol::ptr req) {
                                             register_client->recvResponse(request->m_msg_id,
                                                                           [&register_client, request, rpc_client, service_name](
                                                                                   Protocol::ptr rsp) {
                                                                               // 更新本地缓存
                                                                               auto server_list_str = rsp->m_body_data_map["server_list"];
                                                                               rpc_client->updateCache(service_name,
                                                                                                       server_list_str);
                                                                               // 获取本地ip地址，根据该ip地址去进行hash选择
                                                                               auto local_ip = getLocalIP();
                                                                               auto server_addr = rpc_client->m_service_balance[service_name]->getServer(
                                                                                       local_ip);
                                                                               DEBUGLOG(
                                                                                       "local ip [%s], choosing server [%s]",
                                                                                       local_ip.c_str(),
                                                                                       server_addr->toString().c_str());
                                                                               // 为服务器创建连接，以后的请求都使用这里的连接
                                                                               rpc_client->m_server_client = std::make_shared<TCPClient>(
                                                                                       server_addr,
                                                                                       rpc_client->m_call_io_thread->getEventLoop(),
                                                                                       rpc_client->m_protocol_type);
                                                                               bool is_connected = false;
                                                                               rpc_client->m_server_client->connect(
                                                                                       [&is_connected]() { is_connected = true; },
                                                                                       true);
                                                                               while (!is_connected) {}
                                                                               INFOLOG("%s | get server cache from register center, server list [%s]",
                                                                                       rsp->m_msg_id.c_str(),
                                                                                       rpc_client->getAllServerList().c_str());
                                                                               register_client->getEventLoop()->stop();
                                                                           });
                                         });
        });
        io_thread->start();
    }

    std::string RPCClient::getAllServerList() {
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

    void RPCClient::subscribe(const std::string &service_name) {
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
                                                               std::bind(&RPCClient::handlePublish,
                                                                         this,
                                                                         std::placeholders::_1,
                                                                         std::placeholders::_2,
                                                                         std::placeholders::_3),
                                                               m_protocol_type);
    }

    void RPCClient::handlePublish(Protocol::ptr request, Protocol::ptr response, Session::ptr session) {
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

    void RPCClient::updateCache(const std::string &service_name, std::string &server_list) {
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
}
