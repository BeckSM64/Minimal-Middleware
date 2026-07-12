#ifndef ITRANSPORT_H
#define ITRANSPORT_H

#include <string>

class ITransport {
public:
    virtual ~ITransport() {}

    virtual bool Startup() = 0;
    virtual void Cleanup() = 0;

    virtual bool Connect(const std::string& host, unsigned short port) = 0;
    virtual bool Bind(unsigned short port) = 0;
    virtual bool Listen(int backlog) = 0;
    virtual int Accept() = 0;

    virtual int Send(const void* data, size_t size) = 0;
    virtual int Recv(void* data, size_t size) = 0;

    virtual void Close() = 0;

    virtual int GetSocket() const = 0;
};

#endif
