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

    void TCPClientPool::getConnectionAsync(std::function<void(TCPClient::ptr)> callback) {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_idle_clients.empty()) {
            auto pooled_client = m_idle_clients.front();
            m_idle_clients.pop();
            // 检查连接是否有效，有效去调用回调函数，即从外面传入的回调函数SendRequest, RecvResponse
            if (pooled_client.m_client->getState() == Connected) {
                m_active_clients.emplace(pooled_client.m_client);
                DEBUGLOG("==== NOT EMPTY ====== %d ==== %d ========", m_idle_clients.size(),
                         m_active_clients.size())
                callback(pooled_client.m_client);
                return;
            }
        }
        // 如果没有可用的空闲连接，且未达到最大连接数，则创建新连接，加入到活跃set中
        if (m_active_clients.size() < m_max_size) {
            createConnectionAsync([this, callback](TCPClient::ptr client) {
                DEBUGLOG("==== EMPTY ======")
                std::unique_lock<std::mutex> lock(m_mutex);
                m_active_clients.emplace(client);
                callback(client);
            });
            return;
        }
        ERRORLOG("Failed to get connection from pool, active [%d], max [%d]",
                 m_active_clients.size(), m_max_size);
        callback(nullptr);
    }

    void TCPClientPool::createConnectionAsync(std::function<void(TCPClient::ptr)> callback) {
        auto client = std::make_shared<TCPClient>(m_peer_addr,
                                                  m_io_thread_pool->getIOThread()->getEventLoop(),
                                                  m_protocol_type);
        // 连接成功后将自己加入到空闲队列中
        client->connect([client, callback]() {
            DEBUGLOG("==== createConnectionAsync =====")
            callback(client);
        }, true);
    }

    void TCPClientPool::releaseClient(TCPClient::ptr client) {
        if (!client) return;
        std::unique_lock<std::mutex> lock(m_mutex);
        // 从活跃连接集合中移除
        auto it = m_active_clients.find(client);
        if (it != m_active_clients.end()) {
            m_active_clients.erase(it);
            // 如果连接仍然有效，则放回空闲队列，否则自动销毁
            if (client->getState() == Connected) {
                DEBUGLOG("==== releaseClient =====")
                m_idle_clients.emplace(client);
            }
        }
    }

    void TCPClientPool::createConnectionSync() {
        auto client = std::make_shared<TCPClient>(m_peer_addr,
                                                  m_io_thread_pool->getIOThread()->getEventLoop(),
                                                  m_protocol_type);
        // 初始创建连接池时采用阻塞式连接
        bool is_connected = false;
        client->connect([&is_connected]() { is_connected = true; }, true);
        while (!is_connected) {}
        if (is_connected) {
            m_idle_clients.emplace(client);
        }
    }
}
