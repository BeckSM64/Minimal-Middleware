#pragma once

#include <string>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "ITransport.h"

class UdpTransport : public ITransport
{
public:
    UdpTransport();
    ~UdpTransport();

    MmwResult Initialize(std::string& hostname, int port) override;
    MmwResult InitializeServer(int port) override;

    MmwResult Send(const std::string& data) override;
    MmwResult Recv(std::string& data) override;

    MmwResult Accept(
        std::atomic<bool>& running,
        ITransport*& client
    ) override;

    void Close() override;

private:
    MmwResult InitializeSockets();
    void CleanupSockets();

    int m_sockFd = -1;
    std::string m_hostname;
    int m_brokerPort = 0;

    sockaddr_in m_serverAddr{};

    static std::mutex s_socketMutex;
    static int s_socketUsers;
};
