#pragma once

#include <cstddef>
#include <sys/types.h>
#include <memory>
#include "ILink.hpp"

class IServer { 
public: 
    virtual ~IServer() = default; 
    virtual bool start() = 0; 
    virtual void stop() = 0; 
    virtual std::unique_ptr<ILink> acceptConnection() = 0; 
    virtual bool isRunning() const = 0; 
};