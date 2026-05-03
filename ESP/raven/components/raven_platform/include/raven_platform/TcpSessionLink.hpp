#pragma once

#include "ILink.hpp"
#include <cstddef>
#include <sys/types.h>

class TcpSessionLink : public ILink {
public:
    explicit TcpSessionLink(int acceptedSocket);
    ~TcpSessionLink() override;

    bool open() override;
    void close() override;
    ssize_t write(const void* data, size_t len) override;
    ssize_t read(void* buffer, size_t len) override;
    bool isOpen() const override;

private:
    int sock_ = -1;
};