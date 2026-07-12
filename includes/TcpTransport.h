#ifndef TCPTRANSPORT_H
#define TCPTRANSPORT_H

#include "ITransport.h"
#include "SocketAbstraction.h"

class TcpTransport : public ITransport {
public:
    TcpTransport();
    explicit TcpTransport(int socketFd);
    ~TcpTransport() override;

    bool Startup() override;
    void Cleanup() override;

    bool Connect(const std::string& host, unsigned short port) override;
    bool Bind(unsigned short port) override;
    bool Listen(int backlog) override;
    int Accept() override;

    int Send(const void* data, size_t size) override;
    int Recv(void* data, size_t size) override;

    void Close() override;

    int GetSocket() const override;

private:
    int m_socket;
};

#endif
