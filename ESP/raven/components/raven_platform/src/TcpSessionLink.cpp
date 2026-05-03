#include "raven_platform/TcpSessionLink.hpp"

#include "lwip/sockets.h"

TcpSessionLink::TcpSessionLink(int acceptedSocket)
    : sock_(acceptedSocket) {}

TcpSessionLink::~TcpSessionLink() {
    close();
}

bool TcpSessionLink::open() {
    return sock_ >= 0;
}

void TcpSessionLink::close() {
    if (sock_ >= 0) {
        ::shutdown(sock_, SHUT_RDWR);
        ::close(sock_);
        sock_ = -1;
    }
}

ssize_t TcpSessionLink::write(const void* data, size_t len) {
    if (sock_ < 0 || data == nullptr || len == 0) {
        return -1;
    }
    return ::send(sock_, data, len, 0);
}

ssize_t TcpSessionLink::read(void* buffer, size_t len) {
    if (sock_ < 0 || buffer == nullptr || len == 0) {
        return -1;
    }
    return ::recv(sock_, buffer, len, 0);
}

bool TcpSessionLink::isOpen() const {
    return sock_ >= 0;
}