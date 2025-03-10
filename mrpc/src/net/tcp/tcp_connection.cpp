//
// Created by baitianyu on 25-2-10.
//
#include <unistd.h>
#include "net/tcp/tcp_connection.h"
#include "common/log.h"
#include "common/string_util.h"
#include "event/fd_event_pool.h"
#include "common/util.h"

namespace mrpc {

    TCPConnection::TCPConnection(EventLoop::ptr event_loop, NetAddr::ptr local_addr, NetAddr::ptr peer_addr,
                                 int client_fd, int buffer_size, RPCDispatcher::ptr dispatcher,
                                 TCPConnectionType type, ProtocolType protocol_type)
            : m_event_loop(event_loop),
              m_local_addr(local_addr),
              m_peer_addr(peer_addr),
              m_client_fd(client_fd),
              m_state(NotConnected),
              m_dispatcher(dispatcher),
              m_connection_type(type),
              m_protocol_type(protocol_type) {
        if (m_protocol_type == ProtocolType::HTTP_Protocol) {
            m_request_parser = std::make_shared<HTTPRequestParser>();
            m_response_parser = std::make_shared<HTTPResponseParser>();
        } else {
            m_request_parser = std::make_shared<MPbProtocolParser>();
            m_response_parser = std::make_shared<MPbProtocolParser>();
        }
        m_in_buffer = std::make_shared<TCPVectorBuffer>(buffer_size);
        m_out_buffer = std::make_shared<TCPVectorBuffer>(buffer_size);
        m_fd_event = std::make_shared<FDEvent>(client_fd);
        m_fd_event->setNonBlock();
        if (m_connection_type == TCPConnectionByServer) {
            listenRead(); // 本方是服务器的话，监听读
        }
    }

    TCPConnection::~TCPConnection() {
        DEBUGLOG("~TCPConnection");
    }

    void TCPConnection::onRead() {
        if (m_state != Connected) {
            ERRORLOG("onRead error, client has already disconnected, addr [%s], clientfd [%d]",
                     m_peer_addr->toString().c_str(), m_client_fd);
            return;
        }
        // 是否从socket上读取并全部写入到in buffer中
        bool is_read_and_write_all = false;
        bool is_close = false;
        while (!is_read_and_write_all) {
            if (m_in_buffer->writeAbleSize() == 0) {
                m_in_buffer->resizeBuffer(2 * m_in_buffer->getBufferSize());
            }
            int write_count = m_in_buffer->writeAbleSize();
            int write_index = m_in_buffer->getWriteIndex();
            // 返回读取到的字节数
            int ret = read(m_client_fd, &(m_in_buffer->getRefBuffer()[write_index]), write_count);
            DEBUGLOG("success read %d bytes from addr [%s], client fd [%d]", ret, m_peer_addr->toString().c_str(),
                     m_client_fd);
            if (ret > 0) {
                m_in_buffer->moveWriteIndex(ret);
                if (ret == write_count) {
                    continue;
                } else if (ret < write_count) {
                    is_read_and_write_all = true;
                    break;
                }
            } else if (ret == 0) {
                is_close = true;
                break;
            } else if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // 以 O_NONBLOCK的标志打开文件/socket/FIFO，如果连续做read操作而没有数据可读。
                // 此时程序不会阻塞起来等待数据准备就绪返回，read函数会返回一个错误EAGAIN，提示你的应用程序现在没有数据可读请稍后再试。
                is_read_and_write_all = true;
                break;
            } else {
                if (m_connection_type == TCPConnectionByClient) {
                    m_client_error_done(); // 作为客户端，就直接停止执行eventloop，断开client与server的连接
                }
            }
        }
        // 读取完成后不需要取消监听，否则无法判断什么时候ret == 0，即判断关闭
        if (is_close) {
            INFOLOG("peer closed, peer addr [%s], clientfd [%d]", m_peer_addr->toString().c_str(), m_client_fd);
            m_in_buffer->clearBuffer(); // 被动关闭后清空缓冲区
            if (m_connection_type == TCPConnectionByServer)
                clear();
            return;
        }
        if (!is_read_and_write_all) {
            ERRORLOG("not read all data, peer addr [%s], clientfd [%d]", m_peer_addr->toString().c_str(), m_client_fd);
        }
        execute();
    }

    void TCPConnection::execute() {
        // 读取数据
        std::vector<char> tmp;
        m_in_buffer->readFromBuffer(tmp, m_in_buffer->readAbleSize());
        std::string tmp_str(tmp.begin(), tmp.end());
        if (m_connection_type == TCPConnectionByServer) {
            // 服务器端
            // 解析请求
            m_request_parser->parse(tmp_str);

            Protocol::ptr response = nullptr;
            if (m_protocol_type == ProtocolType::HTTP_Protocol) {
                response = std::make_shared<HTTPResponse>();
            } else {
                response = std::make_shared<MPbProtocol>();
            }
            auto session = std::make_shared<Session>(m_local_addr, m_peer_addr);
            // 执行业务并写入response
            m_dispatcher->handle(m_request_parser->getProtocol(), response, session);

            // 编码结果并监听可写
            auto response_str = response->toString();
            m_out_buffer->writeToBuffer(response_str.c_str(), response_str.size());
            listenWrite();
        } else {
            // 客户端
            // 接收response，并调用相应的回调函数
            m_response_parser->parse(tmp_str);
            auto iter = m_read_dones.find(m_response_parser->getProtocol()->m_msg_id);
            if (iter != m_read_dones.end()) {
                auto done = iter->second;
                m_read_dones.erase(iter);
                done(m_response_parser->getProtocol());
            } else {
                DEBUGLOG("not found response_msg_id: %s", m_response_parser->getProtocol()->m_msg_id.c_str());
            }
        }
    }

    void TCPConnection::onWrite() {
        if (m_state != Connected) {
            ERRORLOG("onWrite error, client has already disconnected, addr [%s], clientfd [%d]",
                     m_peer_addr->toString().c_str(), m_client_fd);
            if (m_connection_type == TCPConnectionByServer)
                clear();
            if (m_connection_type == TCPConnectionByClient) {
                m_client_error_done(); // 作为客户端，就直接停止执行eventloop，断开client与server的连接
            }
            return;
        }
        if (m_connection_type == TCPConnectionByClient) {
            // 客户端将请求数据从write done.first中拿出，写入到out buffer中，并进行发送
            // 服务端在execute中就已经写入了out buffer
            for (const auto &item: m_write_dones) {
                auto req = item.first->toString();
                m_out_buffer->writeToBuffer(req.c_str(), req.size());
            }
        }
        bool is_read_and_write_all = false;
        while (!is_read_and_write_all) {
            if (m_out_buffer->readAbleSize() == 0) {
                DEBUGLOG("no more data need to send to client [%s]", m_peer_addr->toString().c_str());
                is_read_and_write_all = true;
                break;
            }
            int read_count = m_out_buffer->readAbleSize();
            int read_index = m_out_buffer->getReadIndex();
            int ret = write(m_client_fd, &(m_out_buffer->getRefBuffer()[read_index]), read_count);
            if (ret >= read_count) {
                DEBUGLOG("no more data need to send to client [%s]", m_peer_addr->toString().c_str());
                is_read_and_write_all = true;
                break;
            } else if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // 发送缓冲区已经满了，已经不可以继续写入
                // 等下一次再发送
                ERRORLOG("write data error, errno EAEAGAIN and rt -1, may be send buffer is full");
                break;
            } else {
                if (m_connection_type == TCPConnectionByServer)
                    clear();
                if (m_connection_type == TCPConnectionByClient) {
                    m_client_error_done(); // 作为客户端，就直接停止执行eventloop，断开client与server的连接
                }
            }
        }
        if (is_read_and_write_all) {
            m_fd_event->cancel_listen(FDEvent::OUT_EVENT);
            m_event_loop->addEpollEvent(m_fd_event);
            m_out_buffer->clearBuffer(); // 关闭后清空缓冲区
        }
        if (m_connection_type == TCPConnectionByClient) {
            for (const auto &write_done: m_write_dones) {
                write_done.second(write_done.first);
            }
            m_write_dones.clear(); // 清空回调函数
        }
    }

    void
    TCPConnection::pushSendMessage(const Protocol::ptr &request, const std::function<void(Protocol::ptr)> &done) {
        m_write_dones.emplace_back(request, done);
    }

    void TCPConnection::pushReadMessage(const std::string &msg_id, const std::function<void(Protocol::ptr)> &done) {
        m_read_dones.emplace(msg_id, done);
    }

    void TCPConnection::setState(TCPState new_state) {
        m_state = new_state;
    }

    TCPState TCPConnection::getState() {
        return m_state;
    }

    void TCPConnection::clear() {
        // 取消所有监听，并设置state
        m_fd_event->cancel_listen(FDEvent::IN_EVENT);
        m_fd_event->cancel_listen(FDEvent::OUT_EVENT);
        m_event_loop->deleteEpollEvent(m_fd_event);
        m_state = Closed;
        close(m_fd_event->getFD());
        close(m_client_fd);
    }

    int TCPConnection::getFD() {
        return m_client_fd;
    }

    void TCPConnection::shutdown() {
        if (m_state == Closed || m_state == NotConnected) {
            return;
        }
        m_state = HalfClosing;
        ::shutdown(m_client_fd, SHUT_RDWR);
    }

    void TCPConnection::setConnectionType(const TCPConnectionType new_type) {
        m_connection_type = new_type;
    }

    void TCPConnection::listenWrite() {
        m_fd_event->listen(FDEvent::OUT_EVENT, std::bind(&TCPConnection::onWrite, this));
        m_event_loop->addEpollEvent(m_fd_event);
    }

    void TCPConnection::listenRead() {
        m_fd_event->listen(FDEvent::IN_EVENT, std::bind(&TCPConnection::onRead, this));
        m_event_loop->addEpollEvent(m_fd_event);
        // 在TCPServer接受连接后，TCPServer的main eventloop构造TCPConnection，并设置listenRead，此时这个IO线程还在epoll
        // 因此需要wakeupIO线程，即主线程通过向eventfd写入事件来唤醒这个线程，并写入到这个线程的pending events中，线程被唤醒后
        // 执行pending events，随后执行将m_fd_event添加到自己的epoll中的事件
    }

    NetAddr::ptr TCPConnection::getLocalAddr() {
        return m_local_addr;
    }

    NetAddr::ptr TCPConnection::getPeerAddr() {
        return m_peer_addr;
    }

}

