#include "mcp/network/inet_address.h"

#include <cstring>

#include <arpa/inet.h>

namespace mcp {

InetAddress::InetAddress(uint16_t port, const std::string& ip) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);

    if (ip.empty() || ip == "0.0.0.0") {
        addr_.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
    }
}

std::string InetAddress::toIp() const {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

std::string InetAddress::toIpPort() const {
    char buf[64];
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    int port_num = ntohs(addr_.sin_port);
    return std::string(buf) + ":" + std::to_string(port_num);
}

} // namespace mcp
