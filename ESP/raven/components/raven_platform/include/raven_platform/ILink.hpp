#pragma once

#include <cstddef>
#include <sys/types.h>

class ILink {
public:
    virtual ~ILink() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual ssize_t write(const void* data, size_t len) = 0;
    virtual ssize_t read(void* buffer, size_t len) = 0;
    virtual bool isOpen() const = 0;
};