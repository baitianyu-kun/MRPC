//
// Created by baitianyu on 25-3-11.
//

#ifndef RPCFRAME_RPC_CLIENT_H
#define RPCFRAME_RPC_CLIENT_H

#include "net/balance/hash_balance.h"
#include "rpc/rpc_publish_listener.h"
#include "net/tcp/tcp_connection_pool.h"

namespace mrpc {
    class RPCClient : public std::enable_shared_from_this<RPCClient> {
    public:
        using ptr = std::shared_ptr<RPCClient>;

        RPCClient();

        ~RPCClient();

        void serviceDiscovery(const std::string &service_name);

        std::string getAllServerList();

        size_t getCacheSize() const { return m_service_servers_cache.size(); }

        ProtocolType getProtocolType() const { return m_protocol_type; }

        TCPClientPool::ptr getTCPClientPool() { return m_tcp_client_pool; }

    public:
        void subscribe(const std::string &service_name);

        void handlePublish(Protocol::ptr request, Protocol::ptr response, Session::ptr session);

    private:
        void updateCache(const std::string &service_name, std::string &server_list);

    private:
        PublishListener::ptr m_publish_listener;

    private:
        TCPClientPool::ptr m_tcp_client_pool;

    private:
        std::unordered_map<std::string, std::set<std::string>> m_service_servers_cache; // service对应的多少个server
        std::unordered_map<std::string, ConsistentHash::ptr> m_service_balance; // 一个service对应一个balance
        NetAddr::ptr m_register_center_addr;
        ProtocolType m_protocol_type;
    };
}

#endif //RPCFRAME_RPC_CLIENT_H
