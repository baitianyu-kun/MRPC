//
// Created by baitianyu on 25-2-10.
//

#ifndef RPCFRAME_TCP_SERVER_H
#define RPCFRAME_TCP_SERVER_H

#include "net/tcp/net_addr.h"
#include "net/tcp/tcp_acceptor.h"
#include "event/eventloop.h"
#include "event/io_thread_pool.h"
#include "net/tcp/tcp_connection.h"

#define CLEAR_CONNECTIONS_INTERVAL Config::GetGlobalConfig()->m_clear_connections_interval

namespace mrpc {
    class TCPServer {
    public:
        using ptr = std::shared_ptr<TCPServer>;

        TCPServer(NetAddr::ptr local_addr, ProtocolType protocol_type = ProtocolType::HTTP_Protocol);

        ~TCPServer();

        void start();

        void addServlet(const std::string &uri, Servlet::ptr slt);

        void addServlet(const std::string &uri, CallBacksServlet::callback cb);

        std::unique_ptr<IOThreadPool> &getIOThreadPool();

        EventLoop::ptr getMainEventLoop();

    private:
        void init();

        // 当有新客户端连接之后需要执行
        void onAccept();

        // 定时清理已经关闭了的客户端连接
        void clearClientTimerFunc();

    protected:
        NetAddr::ptr m_local_addr; // 本地监听地址
        TCPAcceptor::ptr m_acceptor;
        EventLoop::ptr m_main_event_loop;
        std::unique_ptr<IOThreadPool> m_io_thread_pool;
        FDEvent::ptr m_listen_fd_event;
        std::set<TCPConnection::ptr> m_client_connections;
        RPCDispatcher::ptr m_dispatcher;

    protected:
        ProtocolType m_protocol_type;
    };
}

#endif //RPCFRAME_TCP_SERVER_H
