//
// Created by baitianyu on 3/10/25.
//

#ifndef RPCFRAME_TCP_CONNECTION_POOL_H
#define RPCFRAME_TCP_CONNECTION_POOL_H

#include <mutex>
#include "net/tcp/tcp_connection.h"
#include "net/tcp/tcp_client.h"
#include "event/io_thread_pool.h"

#define TCP_CONNECTION_POOL_IO_THREAD_POOL_SIZE 4
#define TCP_CONNECTION_POOL_MIN_SIZE 20
#define TCP_CONNECTION_POOL_MAX_SIZE 50
#define TCP_CONNECTION_POOL_IDLE_TIME 60

namespace mrpc {
    class TCPClientPool {
    public:
        using ptr = std::unique_ptr<TCPClientPool>;

        TCPClientPool(NetAddr::ptr peer_addr,
                      EventLoop::ptr event_loop,
                      ProtocolType protocol_type = ProtocolType::HTTP_Protocol,
                      uint32_t min_size = TCP_CONNECTION_POOL_MIN_SIZE,
                      uint32_t max_size = TCP_CONNECTION_POOL_MAX_SIZE,
                      uint32_t max_idle_time = TCP_CONNECTION_POOL_IDLE_TIME  // 最大空闲时间(秒)
        );

        ~TCPClientPool();

        // 获取一个异步连接，是TCPClient负责发送和接收消息，以及处理回调，所以TCP连接池实际上就是Client连接池
        void getConnectionAsync(std::function<void(TCPClient::ptr)> callback);

        // 归还一个连接
        void releaseClient(TCPClient::ptr client);

        // 异步创建一个新的连接
        void createConnectionAsync(std::function<void(TCPClient::ptr)> callback);

        // 同步创建connection
        void createConnectionSync();

        void checkIdleClients();


    private:
        struct PooledClient {
            using ptr = std::shared_ptr<PooledClient>;
            TCPClient::ptr m_client;
            Timestamp last_used;  // 最后使用时间
            // Return the current time and put it in *TIMER if TIMER is not NULL.
            explicit PooledClient(TCPClient::ptr client)
                    : m_client(client), last_used(Timestamp::now()) {}
        };

    private:
        NetAddr::ptr m_peer_addr;                    // 服务器地址
        EventLoop::ptr m_event_loop;                 // 事件循环
        uint32_t m_min_size;                         // 最小连接数
        uint32_t m_max_size;                         // 最大连接数
        uint32_t m_max_idle_time;                    // 最大空闲时间(秒)
        ProtocolType m_protocol_type;                // 协议类型
        std::queue<PooledClient> m_idle_clients;   // 空闲连接队列
        std::set<TCPClient::ptr> m_active_clients; // 活跃连接集合
        std::mutex m_mutex;                          // 保护连接池的互斥锁
        std::unique_ptr<IOThreadPool> m_io_thread_pool;
    };
}

#endif //RPCFRAME_TCP_CONNECTION_POOL_H
