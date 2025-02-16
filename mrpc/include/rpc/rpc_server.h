//
// Created by baitianyu on 2/10/25.
//

#ifndef RPCFRAME_RPC_SERVER_H
#define RPCFRAME_RPC_SERVER_H

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include "net/tcp/tcp_server.h"
#include "net/tcp/tcp_client.h"

#define HEART_TIMER_EVENT_INTERVAL 5

// Servlet执行业务的时候调用这里的函数
namespace mrpc {
    class RPCServer : public TCPServer {

    private:
        NetAddr::ptr m_local_addr; // 本地监听地址
        NetAddr::ptr m_register_addr; // 注册中心地址
        TimerEventInfo::ptr m_heart_timer_event; // 心跳包定时事件

    public:
        using ptr = std::unique_ptr<RPCServer>;

        RPCServer(NetAddr::ptr local_addr, NetAddr::ptr register_addr);

        ~RPCServer();

        void initServlet();

        void registerToCenter();

        void heartToCenter();

        void startRPC();

    private:
        using protobuf_service_ptr = std::shared_ptr<google::protobuf::Service>;
        std::unordered_map<std::string, protobuf_service_ptr> m_service_maps; // 存储所有service

        static bool
        parseServiceFullName(const std::string &full_name, std::string &service_name, std::string &method_name);

    public:
        void handleService(HTTPRequest::ptr request, HTTPResponse::ptr response, HTTPSession::ptr session);

        void addService(const protobuf_service_ptr &service);

        std::vector<std::string> getAllServiceNames();

        std::string getAllServiceNamesStr();
    };
}

#endif //RPCFRAME_RPC_SERVER_H
