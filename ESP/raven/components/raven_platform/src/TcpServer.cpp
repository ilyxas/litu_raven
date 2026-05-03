#include "raven_platform/TcpServer.hpp"

#include "raven_platform/TcpSessionLink.hpp"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <cstring>

TcpServer::TcpServer(uint16_t port)
    : port_(port) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    if (listen_sock_ >= 0) {
        return true;
    }

    listen_sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock_ < 0) {
        listen_sock_ = -1;
        return false;
    }

    int opt = 1;
    ::setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        stop();
        return false;
    }

    if (::listen(listen_sock_, 1) != 0) {
        stop();
        return false;
    }

    return true;
}

void TcpServer::stop() {
    if (listen_sock_ >= 0) {
        ::shutdown(listen_sock_, SHUT_RDWR);
        ::close(listen_sock_);
        listen_sock_ = -1;
    }
}

std::unique_ptr<ILink> TcpServer::acceptConnection() {
    if (listen_sock_ < 0) {
        return nullptr;
    }

    sockaddr_storage source_addr {};
    socklen_t addr_len = sizeof(source_addr);

    const int sock = ::accept(listen_sock_, reinterpret_cast<sockaddr*>(&source_addr), &addr_len);
    if (sock < 0) {
        return nullptr;
    }

    return std::make_unique<TcpSessionLink>(sock);
}

bool TcpServer::isRunning() const {
    return listen_sock_ >= 0;
}