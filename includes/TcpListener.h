#pragma once

#include "IListener.h"

class TcpListener : public IListener
{
public:
    TcpListener();
    ~TcpListener() override;

    bool Listen(uint16_t port) override;
    std::shared_ptr<ITransport> Accept() override;
    void Close() override;

private:
    int m_listenSocket;
};
