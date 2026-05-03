#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include "ILink.hpp"

class TcpClientLink : public ILink {
public:
    TcpClientLink(const char* host, uint16_t port);

    bool open() override;
    void close() override;
    ssize_t write(const void* data, size_t len) override;
    ssize_t read(void* buffer, size_t len) override;
    bool isOpen() const override;

private:
    const char* host_;
    uint16_t port_;
    int sock_ = -1;
};