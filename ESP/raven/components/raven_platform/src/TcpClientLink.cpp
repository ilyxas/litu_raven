#include "raven_platform/TcpClientLink.hpp"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "arpa/inet.h"

#include <cstring>

TcpClientLink::TcpClientLink(const char* host, uint16_t port)
    : host_(host)
    , port_(port) {
}

bool TcpClientLink::open() {
    if (isOpen()) {
        return true;
    }

    if (host_ == nullptr || host_[0] == '\0') {
        return false;
    }

    sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock_ < 0) {
        sock_ = -1;
        return false;
    }

    sockaddr_in dest_addr {};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_);

    // Сначала пытаемся интерпретировать host как IPv4-строку.
    // Например: "192.168.1.10"
    const int inet_ok = ::inet_pton(AF_INET, host_, &dest_addr.sin_addr);
    if (inet_ok == 1) {
        if (::connect(sock_, reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr)) != 0) {
            close();
            return false;
        }
        return true;
    }

    // Если это не IP-строка, пробуем DNS-резолв.
    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const int err = ::getaddrinfo(host_, nullptr, &hints, &result);
    if (err != 0 || result == nullptr) {
        close();
        return false;
    }

    bool connected = false;

    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        if (rp->ai_family != AF_INET || rp->ai_addr == nullptr) {
            continue;
        }

        auto* resolved = reinterpret_cast<sockaddr_in*>(rp->ai_addr);
        dest_addr.sin_addr = resolved->sin_addr;

        if (::connect(sock_, reinterpret_cast<sockaddr*>(&dest_addr), sizeof(dest_addr)) == 0) {
            connected = true;
            break;
        }
    }

    ::freeaddrinfo(result);

    if (!connected) {
        close();
        return false;
    }

    return true;
}

void TcpClientLink::close() {
    if (sock_ >= 0) {
        ::shutdown(sock_, SHUT_RDWR);
        ::close(sock_);
        sock_ = -1;
    }
}

ssize_t TcpClientLink::write(const void* data, size_t len) {
    if (!isOpen() || data == nullptr || len == 0) {
        return -1;
    }

    return ::send(sock_, data, len, 0);
}

ssize_t TcpClientLink::read(void* buffer, size_t len) {
    if (!isOpen() || buffer == nullptr || len == 0) {
        return -1;
    }

    return ::recv(sock_, buffer, len, 0);
}

bool TcpClientLink::isOpen() const {
    return sock_ >= 0;
}