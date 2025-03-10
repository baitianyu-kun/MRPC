//
// Created by baitianyu on 7/14/24.
//

#ifndef RPCFRAME_CONFIG_H
#define RPCFRAME_CONFIG_H

#include <map>
#include <memory>
#include "tinyxml2.h"


namespace mrpc {

    class Config {
    public:

        explicit Config(const char *xmlfile);

        ~Config();

    public:
        static std::unique_ptr<Config> &GetGlobalConfig();

        static void SetGlobalConfig(const char *xmlfile);

    public:
        // log
        std::string m_log_level;
        std::string m_log_file_name;
        std::string m_log_file_path;
        int m_log_max_file_size{0};
        int m_log_sync_interval{0};   // 日志同步间隔，s

        // server
        int m_io_thread_pool_size{0};
        int m_io_fd_event_pool_size{0};
        double m_clear_connections_interval{0}; // 定时清理连接
        int m_tcp_buffer_size{0}; // tcp buffer大小
        int m_max_connections{0};

        // balance
        int m_virtual_node_num{0}; // 一致性hash虚拟节点数量
        std::string m_network_card_name; // 网卡名称，用于获取本机ip地址

        // channel
        std::string m_channel_peer_register_ip;
        int m_channel_peer_register_port;

        // register center
        int m_server_time_out{0}; // 服务器超时时间
        std::string m_register_listen_ip;
        int m_register_listen_port{0};

        // rpc_server
        double m_heart_pack_interval{0}; // 发送heart pack的延时
        std::string m_rpc_server_listen_ip;
        int m_rpc_server_listen_port{0};
        std::string m_server_peer_register_ip;
        int m_server_peer_register_port{0};

        // path
        std::string m_rpc_method_path;
        std::string m_rpc_register_heart_server_path;
        std::string m_rpc_server_register_path;
        std::string m_rpc_client_register_discovery_path;
        std::string m_register_subscribe_path;
        std::string m_register_publish_path;

        std::string m_protocol;

    private:
        std::unique_ptr<tinyxml2::XMLDocument> m_xml_document{nullptr};

        tinyxml2::XMLElement *root_node;

        void initLog();

        void initServer();

        void initChannel();

        void initRegister();

        void initRPCServer();

        void initBalance();

        void initPath();

        void readConfig();
    };


}

#endif //RPCFRAME_CONFIG_H
