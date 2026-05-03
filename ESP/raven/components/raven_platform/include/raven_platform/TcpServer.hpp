#pragma once

#include "IServer.hpp"
#include <cstdint>
#include <memory>

class TcpServer : public IServer {
public:
    explicit TcpServer(uint16_t port);
    ~TcpServer() override;

    bool start() override;
    void stop() override;
    std::unique_ptr<ILink> acceptConnection() override;
    bool isRunning() const override;

private:
    uint16_t port_;
    int listen_sock_ = -1;
};