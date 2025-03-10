//
// Created by baitianyu on 7/14/24.
//
#include "common/config.h"

// 使用#把宏参数变为一个字符串，用##把两个宏参数贴合在一起。
#define READ_XML_NODE(name, parent) \
    tinyxml2::XMLElement* name##_node = parent->FirstChildElement(#name); \
    if (!name##_node) { \
        printf("Start mrpc server error, failed to read node [%s]\n", #name); \
        exit(0); \
    } \

#define READ_STR_FROM_XML_NODE(name, parent) \
    tinyxml2::XMLElement* name##_node = parent->FirstChildElement(#name); \
    if (!name##_node|| !name##_node->GetText()) { \
        printf("Start mrpc server error, failed to read config file %s\n", #name); \
        exit(0); \
    } \
    std::string name##_str = std::string(name##_node->GetText()); \

namespace mrpc {

    static std::unique_ptr<Config> g_config = nullptr;

    std::unique_ptr<Config> &Config::GetGlobalConfig() {
        if (g_config == nullptr) {
            printf("Please set global config path.\n");
        }
        return g_config;
    }

    void Config::SetGlobalConfig(const char *xmlfile) {
        if (g_config == nullptr) {
            g_config = std::make_unique<Config>(xmlfile);
        }
    }

    Config::Config(const char *xmlfile) {
        m_xml_document = std::make_unique<tinyxml2::XMLDocument>();
        auto error = m_xml_document->LoadFile(xmlfile);
        if (error != tinyxml2::XMLError::XML_SUCCESS) {
            printf("Start mrpc server error, failed to read config file %s \n", xmlfile);
            exit(0);
        }
        readConfig();
    }

    Config::~Config() {}

    void Config::initLog() {
        READ_XML_NODE(log, root_node);
        READ_STR_FROM_XML_NODE(log_level, log_node);
        READ_STR_FROM_XML_NODE(log_file_name, log_node);
        READ_STR_FROM_XML_NODE(log_file_path, log_node);
        READ_STR_FROM_XML_NODE(log_max_file_size, log_node);
        READ_STR_FROM_XML_NODE(log_sync_interval, log_node);
        m_log_level = log_level_str;
        m_log_file_name = log_file_name_str;
        m_log_file_path = log_file_path_str;
        m_log_max_file_size = std::stoi(log_max_file_size_str);
        m_log_sync_interval = std::stoi(log_sync_interval_str);
        printf("LOG -- CONFIG LEVEL [%s], FILE_NAME [%s], FILE_PATH [%s], MAX_FILE_SIZE [%d Byte], SYNC_INTERVAL [%d s]\n",
               m_log_level.c_str(), m_log_file_name.c_str(), m_log_file_path.c_str(), m_log_max_file_size,
               m_log_sync_interval);

    }

    void Config::initServer() {
        READ_XML_NODE(server, root_node);
        READ_STR_FROM_XML_NODE(io_thread_pool_size, server_node);
        READ_STR_FROM_XML_NODE(io_fd_event_pool_size, server_node);
        READ_STR_FROM_XML_NODE(clear_connections_interval, server_node);
        READ_STR_FROM_XML_NODE(tcp_buffer_size, server_node);
        READ_STR_FROM_XML_NODE(max_connections, server_node);
        m_io_thread_pool_size = std::stoi(io_thread_pool_size_str);
        m_io_fd_event_pool_size = std::stoi(io_fd_event_pool_size_str);
        m_clear_connections_interval = std::stod(clear_connections_interval_str);
        m_tcp_buffer_size = std::stoi(tcp_buffer_size_str);
        m_max_connections = std::stoi(max_connections_str);
    }

    void Config::initRPCServer() {
        READ_XML_NODE(rpc_server, root_node);
        READ_STR_FROM_XML_NODE(heart_pack_interval, rpc_server_node);
        READ_STR_FROM_XML_NODE(rpc_server_listen_ip, rpc_server_node);
        READ_STR_FROM_XML_NODE(rpc_server_listen_port, rpc_server_node);
        READ_STR_FROM_XML_NODE(server_peer_register_ip, rpc_server_node);
        READ_STR_FROM_XML_NODE(server_peer_register_port, rpc_server_node);
        m_heart_pack_interval = std::stod(heart_pack_interval_str);
        m_rpc_server_listen_ip = rpc_server_listen_ip_str;
        m_rpc_server_listen_port = std::stoi(rpc_server_listen_port_str);
        m_server_peer_register_ip = server_peer_register_ip_str;
        m_server_peer_register_port = std::stoi(server_peer_register_port_str);
    }

    void Config::initChannel() {
        READ_XML_NODE(channel, root_node);
        READ_STR_FROM_XML_NODE(channel_peer_register_ip, channel_node);
        READ_STR_FROM_XML_NODE(channel_peer_register_port, channel_node);
        m_channel_peer_register_ip = channel_peer_register_ip_str;
        m_channel_peer_register_port = std::stoi(channel_peer_register_port_str);
    }

    void Config::initRegister() {
        READ_XML_NODE(register_center, root_node);
        READ_STR_FROM_XML_NODE(server_time_out, register_center_node);
        READ_STR_FROM_XML_NODE(register_center_listen_ip, register_center_node);
        READ_STR_FROM_XML_NODE(register_center_listen_port, register_center_node);
        m_server_time_out = std::stoi(server_time_out_str);
        m_register_listen_ip = register_center_listen_ip_str;
        m_register_listen_port = std::stoi(register_center_listen_port_str);
    }

    void Config::initBalance() {
        READ_XML_NODE(balance, root_node);
        READ_STR_FROM_XML_NODE(virtual_node_num, balance_node);
        READ_STR_FROM_XML_NODE(network_card_name, balance_node);
        m_virtual_node_num = std::stoi(virtual_node_num_str);
        m_network_card_name = network_card_name_str;
    }

    void Config::initPath() {
        READ_XML_NODE(path, root_node);
        READ_STR_FROM_XML_NODE(rpc_method_path, path_node);
        READ_STR_FROM_XML_NODE(rpc_register_heart_server_path, path_node);
        READ_STR_FROM_XML_NODE(rpc_server_register_path, path_node);
        READ_STR_FROM_XML_NODE(rpc_client_register_discovery_path, path_node);
        READ_STR_FROM_XML_NODE(register_subscribe_path, path_node);
        READ_STR_FROM_XML_NODE(register_publish_path, path_node);
        m_rpc_method_path = rpc_method_path_str;
        m_rpc_register_heart_server_path = rpc_register_heart_server_path_str;
        m_rpc_server_register_path = rpc_server_register_path_str;
        m_rpc_client_register_discovery_path = rpc_client_register_discovery_path_str;
        m_register_subscribe_path = register_subscribe_path_str;
        m_register_publish_path = register_publish_path_str;
    }

    void Config::readConfig() {
        root_node = m_xml_document->FirstChildElement("root");
        if (!root_node) {
            printf("Start mrpc server error, failed to read node root\n");
            exit(0);
        }
        READ_STR_FROM_XML_NODE(protocol, root_node);
        m_protocol = protocol_str;
        initLog();
        initBalance();
        initChannel();
        initPath();
        initServer();
        initRPCServer();
        initRegister();
    }
}