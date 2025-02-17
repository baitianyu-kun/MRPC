//
// Created by baitianyu on 25-2-12.
//
#include "rpc/register_center.h"

using namespace mrpc;

int main() {
    Config::SetGlobalConfig("../conf/mrpc.xml");
    Logger::InitGlobalLogger(0);
    auto local_addr = std::make_shared<IPNetAddr>(Config::GetGlobalConfig()->m_register_listen_ip,
                                                  Config::GetGlobalConfig()->m_register_listen_port);
    auto register_center = std::make_shared<RegisterCenter>(local_addr);
    register_center->startRegisterCenter();
    return 0;
}