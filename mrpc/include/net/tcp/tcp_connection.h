//
// Created by baitianyu on 25-2-10.
//

#ifndef RPCFRAME_TCP_CONNECTION_H
#define RPCFRAME_TCP_CONNECTION_H

#include <memory>
#include <coroutine>
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

        struct RecvAwaiter {
            TCPConnection* conn;
            std::string msg_id;
            Protocol::ptr result;

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) noexcept {
                conn->m_read_dones.emplace(msg_id, std::make_pair(h, &result));
                conn->listenRead();
            }
            Protocol::ptr await_resume() noexcept {
                return result;
            }
        };

        struct SendAwaiter {
            TCPConnection* conn;
            Protocol::ptr request;

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) noexcept {
                conn->m_write_dones.emplace_back(request, h);
                conn->listenWrite();
            }
            void await_resume() noexcept {}
        };

        SendAwaiter sendRequest(const Protocol::ptr &request) {
            return {this, request};
        }

        RecvAwaiter recvResponse(const std::string &msg_id) {
            return {this, msg_id, nullptr};
        }

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

        void resetNew() {
            m_read_dones.clear();
            m_write_dones.clear();
            listenRead();
        }

    public:
        // 客户端收到信息后，根据msg id找到对应的response的协程句柄和结果指针
        std::unordered_map<std::string, std::pair<std::coroutine_handle<>, Protocol::ptr*>> m_read_dones;

        // key是request，value是对应的协程句柄
        std::vector<std::pair<Protocol::ptr, std::coroutine_handle<>>> m_write_dones;

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

        // 客户端收发数据过程中出现错误需要进行处理的函数
        std::function<void()> m_client_error_done;
    };


}

#endif //RPCFRAME_TCP_CONNECTION_H
