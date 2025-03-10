//
// Created by baitianyu on 2/10/25.
//
#include "rpc/rpc_server.h"
#include "net/protocol/http/http_define.h"
#include "rpc/rpc_controller.h"

namespace mrpc {

    RPCServer::RPCServer(NetAddr::ptr local_addr, NetAddr::ptr register_addr, ProtocolType protocol_type)
            : TCPServer(local_addr, protocol_type),
              m_local_addr(local_addr),
              m_register_addr(register_addr) {
        Timestamp timestamp(addTime(Timestamp::now(), HEART_TIMER_EVENT_INTERVAL));
        auto new_timer_id = getMainEventLoop()->addTimerEvent(std::bind(&RPCServer::heartToCenter, this), timestamp,
                                                              HEART_TIMER_EVENT_INTERVAL);
        initServlet();
    }

    RPCServer::~RPCServer() {
        DEBUGLOG("~RPCServer");
    }

    void RPCServer::initServlet() {
        // 客户端访问服务器
        addServlet(RPC_METHOD_PATH, std::bind(&RPCServer::handleService, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2,
                                              std::placeholders::_3));
    }

    void RPCServer::heartToCenter() {
        auto io_thread = std::make_unique<IOThread>();
        auto client = std::make_shared<TCPClient>(m_register_addr, io_thread->getEventLoop(), m_protocol_type);
        body_type body;
        body["server_ip"] = m_local_addr->getStringIP();
        body["server_port"] = m_local_addr->getStringPort();
        Protocol::ptr request = nullptr;
        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            request = std::make_shared<HTTPRequest>();
            HTTPManager::createRequest(std::static_pointer_cast<HTTPRequest>(request),
                                       MSGType::RPC_REGISTER_HEART_SERVER_REQUEST, body);
        } else {
            request = std::make_shared<MPbProtocol>();
            MPbManager::createRequest(std::static_pointer_cast<MPbProtocol>(request),
                                      MSGType::RPC_REGISTER_HEART_SERVER_REQUEST, body);
        }
        client->connect([&client, request]() {
            client->sendRequest(request, [&client, request](Protocol::ptr req) {
                client->recvResponse(request->m_msg_id, [&client, request](Protocol::ptr rsp) {
                    INFOLOG("%s | success heart to center, peer addr [%s], local addr[%s]",
                            rsp->m_msg_id.c_str(),
                            client->getPeerAddr()->toString().c_str(),
                            client->getLocalAddr()->toString().c_str());
                    client->getEventLoop()->stop();
                });
            });
        });
        io_thread->start();
    }

    void RPCServer::registerToCenter() {
        auto io_thread = std::make_unique<IOThread>();
        // 放到线程里执行，因为TCPClient里面的TCPConnection用的eventloop和当前RPCServer是一个(因为都是主线程)，所以把这个
        // 函数放到子线程中去执行，就不是一个eventloop了，防止造成影响
        auto client = std::make_shared<TCPClient>(m_register_addr, io_thread->getEventLoop(), m_protocol_type);
        body_type body;
        body["server_ip"] = m_local_addr->getStringIP();
        body["server_port"] = m_local_addr->getStringPort();
        body["all_services_names"] = getAllServiceNamesStr();
        Protocol::ptr request = nullptr;
        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            request = std::make_shared<HTTPRequest>();
            HTTPManager::createRequest(std::static_pointer_cast<HTTPRequest>(request),
                                       MSGType::RPC_SERVER_REGISTER_REQUEST, body);
        } else {
            request = std::make_shared<MPbProtocol>();
            MPbManager::createRequest(std::static_pointer_cast<MPbProtocol>(request),
                                      MSGType::RPC_SERVER_REGISTER_REQUEST, body);
        }
        client->connect([&client, request]() {
            client->sendRequest(request, [&client, request](Protocol::ptr req) {
                client->recvResponse(request->m_msg_id, [&client, request](Protocol::ptr rsp) {
                    client->getEventLoop()->stop();
                    INFOLOG("%s | success register to center, peer addr [%s], local addr[%s], add_service_count [%s]",
                            rsp->m_msg_id.c_str(),
                            client->getPeerAddr()->toString().c_str(),
                            client->getLocalAddr()->toString().c_str(),
                            rsp->m_body_data_map["add_service_count"].c_str());
                });
            });
        });
        io_thread->start();
    }

    void RPCServer::startRPC() {
        registerToCenter();
        start();
    }

    void RPCServer::handleService(Protocol::ptr request, Protocol::ptr response, Session::ptr session) {
        // 处理具体业务
        auto method_full_name = request->m_body_data_map["method_full_name"];
        auto pb_data = request->m_body_data_map["pb_data"];
        std::string service_name;
        std::string method_name;
        if (!parseServiceFullName(method_full_name, service_name, method_name)) {
            ERRORLOG("no such service like %s.", method_full_name.c_str());
            return;
        }

        auto iter = m_service_maps.find(service_name);
        if (iter == m_service_maps.end()) {
            ERRORLOG("%s | service name [%s] not found", request->m_msg_id.c_str(), service_name.c_str());
            return;
        }

        auto service = (*iter).second;
        auto method = service->GetDescriptor()->FindMethodByName(method_name);
        if (method == nullptr) {
            ERRORLOG("%s | method name [%s] not found in service [%s]", request->m_msg_id.c_str(),
                     method_name.c_str(), service_name.c_str());
            return;
        }

        auto request_rpc_message = std::shared_ptr<google::protobuf::Message>(
                service->GetRequestPrototype(method).New());
        if (!request_rpc_message->ParseFromString(pb_data)) {
            ERRORLOG("%s | deserialize error", request->m_msg_id.c_str(), method_name.c_str(),
                     service_name.c_str());
            return;
        }

        INFOLOG("%s | get rpc request [%s]", request->m_msg_id.c_str(), method_full_name.c_str());

        auto response_rpc_message = std::shared_ptr<google::protobuf::Message>(
                service->GetResponsePrototype(method).New());

        auto controller = std::make_shared<RPCController>();
        controller->SetLocalAddr(session->getLocalAddr());
        controller->SetPeerAddr(session->getPeerAddr());
        controller->SetMsgId(request->m_msg_id);

        service->CallMethod(method, controller.get(), request_rpc_message.get(), response_rpc_message.get(), nullptr);

        std::string res_pb_data;
        response_rpc_message->SerializeToString(&res_pb_data);

        body_type body;
        body["method_full_name"] = method_full_name;
        body["pb_data"] = res_pb_data;
        body["msg_id"] = request->m_msg_id;

        DEBUGLOG("===== RES MSG ID %s =====", request->m_msg_id.c_str());

        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            HTTPManager::createResponse(std::static_pointer_cast<HTTPResponse>(response), MSGType::RPC_METHOD_RESPONSE,
                                        body);
        } else {
            MPbManager::createResponse(std::static_pointer_cast<MPbProtocol>(response), MSGType::RPC_METHOD_RESPONSE,
                                       body);
        }

        INFOLOG("%s | http dispatch success, request [%s], response [%s]",
                request->m_msg_id.c_str(), request_rpc_message->ShortDebugString().c_str(),
                response_rpc_message->ShortDebugString().c_str());
    }

    bool
    RPCServer::parseServiceFullName(const std::string &full_name, std::string &service_name, std::string &method_name) {
        if (full_name.empty()) {
            ERRORLOG("full name empty");
            return false;
        }
        // Order.makeOrder
        auto i = full_name.find_first_of(".");
        if (i == full_name.npos) {
            ERRORLOG("not find '.' in full name [%s]", full_name.c_str());
            return false;
        }
        service_name = full_name.substr(0, i);
        method_name = full_name.substr(i + 1, full_name.length() - i - 1);
        INFOLOG("parse service_name [%s] and method_name [%s] from full name [%s]", service_name.c_str(),
                method_name.c_str(), full_name.c_str());
        return true;
    }

    void RPCServer::addService(const RPCServer::protobuf_service_ptr &service) {
        auto service_name = service->GetDescriptor()->full_name();
        m_service_maps[service_name] = service;
    }

    std::vector<std::string> RPCServer::getAllServiceNames() {
        std::vector<std::string> tmp_service_names;
        for (const auto &item: m_service_maps) {
            tmp_service_names.push_back(item.first);
        }
        return tmp_service_names;
    }

    std::string RPCServer::getAllServiceNamesStr() {
        std::string all_service_names_str = "";
        for (const auto &item: getAllServiceNames()) {
            all_service_names_str += item;
            all_service_names_str += ",";
        }
        return all_service_names_str;
    }
}

