#pragma once
#include "MMW.h"

class ITransport {
public:
    virtual MmwResult Initialize(std::string& hostname, int port) = 0;
    virtual MmwResult InitializeServer(int port) = 0;
    virtual MmwResult Send(const std::string& data) = 0;
    virtual MmwResult Recv(std::string& data) = 0;
    virtual MmwResult Accept(std::atomic<bool>& running, ITransport*& client) = 0;
    virtual void Close() = 0;
protected:
    int m_brokerPort;
    std::string m_hostname;
};
