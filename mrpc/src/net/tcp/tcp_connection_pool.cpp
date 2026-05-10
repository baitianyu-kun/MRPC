//
// Created by baitianyu on 3/10/25.
//
#include "net/tcp/tcp_connection_pool.h"
#include "common/log.h"
#include "event/io_thread.h"

namespace mrpc {

    TCPClientPool::TCPClientPool(NetAddr::ptr peer_addr,
                                 EventLoop::ptr event_loop,
                                 ProtocolType protocol_type,
                                 uint32_t min_size,
                                 uint32_t max_size,
                                 uint32_t max_idle_time  // 最大空闲时间(秒)
    )
            : m_peer_addr(peer_addr),
              m_event_loop(event_loop),
              m_min_size(min_size),
              m_max_size(max_size),
              m_max_idle_time(max_idle_time),
              m_protocol_type(protocol_type) {
        m_io_thread_pool = std::make_unique<IOThreadPool>(TCP_CONNECTION_POOL_IO_THREAD_POOL_SIZE);
        m_io_thread_pool->start();
        // 初始化最小连接数
        for (uint32_t i = 0; i < m_min_size; ++i) {
            createConnectionSync();
        }
        INFOLOG("Initialize client pool with %d connections", m_idle_clients.size());
    }

    TCPClientPool::~TCPClientPool() {
        std::unique_lock<std::mutex> lock(m_mutex);
        // 清理空闲连接
        while (!m_idle_clients.empty()) {
            auto conn = m_idle_clients.front().m_client;
            m_idle_clients.pop();
        }
        // 清理活跃连接
        m_active_clients.clear();
    }

    mrpc::Task<TCPClient::ptr> TCPClientPool::getConnection() {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_idle_clients.empty()) {
            auto pooled_client = m_idle_clients.front();
            m_idle_clients.pop();
            if (pooled_client.m_client->getState() == Connected) {
                DEBUGLOG("=== NOT EMPTY === %d %d",m_idle_clients.size(),m_active_clients.size())
                m_active_clients.emplace(pooled_client.m_client);
                co_return pooled_client.m_client;
            }
        }
        ERRORLOG("Failed to get connection from pool, active [%d], max [%d]",
                 m_active_clients.size(), m_max_size);
        co_return nullptr;
    }

    mrpc::Task<TCPClient::ptr> TCPClientPool::createConnection() {
        auto client = std::make_shared<TCPClient>(m_peer_addr,
                                                  m_io_thread_pool->getIOThread()->getEventLoop(),
                                                  m_protocol_type);
        bool success = co_await client->connect();
        if (success) {
            co_return client;
        }
        co_return nullptr;
    }

    void TCPClientPool::releaseClient(TCPClient::ptr client) {
        if (!client) return;
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_active_clients.find(client);
        if (it != m_active_clients.end()) {
            m_active_clients.erase(it);
            if (client->getState() == Connected) {
                m_idle_clients.emplace(client);
            }
        }
    }

    void TCPClientPool::createConnectionSync() {
        auto client = std::make_shared<TCPClient>(m_peer_addr,
                                                  m_io_thread_pool->getIOThread()->getEventLoop(),
                                                  m_protocol_type);
        bool is_connected = false;
        auto task_runner = [&]() -> mrpc::Task<void> {
            is_connected = co_await client->connect();
        };
        task_runner();
        while (!is_connected && client->getState() != Closed) {}
        if (is_connected) {
            m_idle_clients.emplace(client);
        }
    }

    void TCPClientPool::checkIdleClients() {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = Timestamp::now();
        std::queue<PooledClient> temp_queue;
        while (!m_idle_clients.empty()) {
            auto pooled_client = m_idle_clients.front();
            m_idle_clients.pop();
            if ((now - pooled_client.last_used > m_max_idle_time) > m_max_idle_time ||
                pooled_client.m_client->getState() != Connected) {
                continue;
            }
            temp_queue.emplace(pooled_client);
        }
        m_idle_clients = std::move(temp_queue);
        while ((m_idle_clients.size() + m_active_clients.size()) < m_min_size) {
            auto runner = [this]() -> mrpc::Task<void> {
                auto client = co_await createConnection();
                if (client) {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_active_clients.emplace(client);
                }
            };
            runner();
        }
    }
}
