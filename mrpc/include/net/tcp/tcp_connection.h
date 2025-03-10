//
// Created by baitianyu on 25-2-10.
//

#ifndef RPCFRAME_TCP_CONNECTION_H
#define RPCFRAME_TCP_CONNECTION_H

#include <memory>
#include "net/tcp/net_addr.h"
#include "net/tcp/tcp_vector_buffer.h"
#include "net/protocol/parse.h"
#include "net/protocol/http/http_parse.h"
#include "net/protocol/mpb/mpb_parse.h"
#include "event/eventloop.h"
#include "rpc/rpc_dispatcher.h"

namespace mrpc {
    enum TCPState {
        NotConnected = 1,
        Connected = 2,
        HalfClosing = 3,
        Closed = 4
    };

    enum TCPConnectionType {
        TCPConnectionByServer = 1,  // 作为服务端使用，代表跟对端客户端的连接
        TCPConnectionByClient = 2,  // 作为客户端使用，代表跟对端服务端的连接
    };

    class TCPConnection {
    public:
        using ptr = std::shared_ptr<TCPConnection>;
    public:
        TCPConnection(EventLoop::ptr event_loop,
                      NetAddr::ptr local_addr,
                      NetAddr::ptr peer_addr,
                      int client_fd,
                      int buffer_size,
                      RPCDispatcher::ptr dispatcher,
                      TCPConnectionType type = TCPConnectionByServer,
                      ProtocolType protocol_type = ProtocolType::HTTP_Protocol);

        ~TCPConnection();

        void onRead();

        void execute();

        void onWrite();

        void pushSendMessage(const Protocol::ptr &request,
                             const std::function<void(Protocol::ptr)> &done);

        void pushReadMessage(const std::string &msg_id,
                             const std::function<void(Protocol::ptr)> &done);

        void setState(TCPState new_state);

        TCPState getState();

        void clear();

        int getFD();

        void shutdown();

        void setConnectionType(TCPConnectionType type);

        void listenWrite();

        void listenRead();

        void setClientErrorCallback(
                const std::function<void()> &client_error_done) { m_client_error_done = client_error_done; }

        NetAddr::ptr getLocalAddr();

        NetAddr::ptr getPeerAddr();

        void cleardones() {
            m_read_dones.clear();
            m_write_dones.clear();
        }

        void clearbuffer() {
            m_in_buffer.reset();
            m_out_buffer.reset();
            m_in_buffer = std::make_shared<TCPVectorBuffer>(2048);
            m_out_buffer = std::make_shared<TCPVectorBuffer>(2048);
        }

    private:
        EventLoop::ptr m_event_loop;
        NetAddr::ptr m_local_addr;
        NetAddr::ptr m_peer_addr;
        TCPVectorBuffer::ptr m_in_buffer; // 接收缓冲区
        TCPVectorBuffer::ptr m_out_buffer; // 发送缓冲区
        FDEvent::ptr m_fd_event{nullptr};
        TCPState m_state;
        int m_client_fd{0};
        TCPConnectionType m_connection_type{TCPConnectionByServer};
        RPCDispatcher::ptr m_dispatcher{nullptr};

        ProtocolParser::ptr m_request_parser;
        ProtocolParser::ptr m_response_parser;
        ProtocolType m_protocol_type;

        // 客户端收到信息后，根据msg id找到对应的response的回调函数，在回调函数里可以判断response的一些状态，比如是否成功，是否获取到相应数据
        std::unordered_map<std::string, std::function<void(Protocol::ptr)>> m_read_dones;

        // key是request，value是该request对应的回调函数
        std::vector<std::pair<Protocol::ptr, std::function<void(Protocol::ptr)>>> m_write_dones;

        // 客户端收发数据过程中出现错误需要进行处理的函数
        std::function<void()> m_client_error_done;
    };


}

#endif //RPCFRAME_TCP_CONNECTION_H
