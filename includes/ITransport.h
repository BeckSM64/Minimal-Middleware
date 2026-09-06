#pragma once
#include "MMW.h"

class ITransport {
public:
    virtual MmwResult Initialize() = 0;
    virtual MmwResult InitializeServer() = 0;
    virtual MmwResult Send(const std::string& data) = 0;
    virtual MmwResult Recv(std::string& data) = 0;
    virtual MmwResult Accept(std::atomic<bool>& running, ITransport*& client) = 0;
    virtual void Close() = 0;
protected:
    int m_brokerPort = 5000;
    std::string m_hostname = "127.0.0.1";
};
