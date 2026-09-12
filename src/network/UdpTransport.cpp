#include <spdlog/spdlog.h>

#include "UdpTransport.h"
#include "SocketAbstraction.h"
#include "MMW.h"

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

std::mutex UdpTransport::s_socketMutex;
int UdpTransport::s_socketUsers = 0;

MmwResult UdpTransport::InitializeSockets()
{
    std::lock_guard<std::mutex> lock(s_socketMutex);

    if (s_socketUsers == 0) {
        if (SocketAbstraction::SocketStartup() != 0) {
            return MMW_ERROR;
        }
    }

    ++s_socketUsers;
    return MMW_OK;
}

void UdpTransport::CleanupSockets()
{
    std::lock_guard<std::mutex> lock(s_socketMutex);

    if (s_socketUsers > 0) {
        --s_socketUsers;

        if (s_socketUsers == 0) {
            SocketAbstraction::SocketCleanup();
        }
    }
}

UdpTransport::UdpTransport()
{
    m_sockFd = -1;
}

UdpTransport::~UdpTransport()
{
    Close();
}

MmwResult UdpTransport::Initialize(std::string& hostname, int port)
{
    m_hostname = hostname;
    m_brokerPort = port;

    if (InitializeSockets() == MMW_ERROR) {
        return MMW_ERROR;
    }

    m_sockFd = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_sockFd == -1) {
        spdlog::error("Failed to create UDP socket");
        CleanupSockets();
        return MMW_ERROR;
    }

    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(m_brokerPort);

    int rc = SocketAbstraction::InetPtonAbstraction(
        AF_INET,
        m_hostname.c_str(),
        &m_serverAddr.sin_addr
    );

    if (rc != 1) {
        spdlog::error("Invalid IP address provided: {}", m_hostname);
        SocketAbstraction::SocketClose(m_sockFd);
        m_sockFd = -1;
        CleanupSockets();
        return MMW_ERROR;
    }

    if (connect(
        m_sockFd,
        (struct sockaddr*)&m_serverAddr,
        sizeof(m_serverAddr)
    ) < 0) {
        spdlog::error("Failed to connect UDP socket to broker");
        SocketAbstraction::SocketClose(m_sockFd);
        m_sockFd = -1;
        CleanupSockets();
        return MMW_ERROR;
    }

    return MMW_OK;
}

MmwResult UdpTransport::InitializeServer(int port)
{
    m_hostname = "0.0.0.0";
    m_brokerPort = port;

    if (InitializeSockets() == MMW_ERROR) {
        return MMW_ERROR;
    }

    m_sockFd = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_sockFd == -1) {
        spdlog::error("Failed to create UDP socket");
        CleanupSockets();
        return MMW_ERROR;
    }

    int opt = 1;

    if (SocketAbstraction::SetSockOpt(
        m_sockFd,
        SOL_SOCKET,
        SO_REUSEADDR,
        (const char*)&opt,
        sizeof(opt)
    ) < 0) {
        spdlog::warn("Failed to set SO_REUSEADDR");
    }

    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_addr.s_addr = INADDR_ANY;
    m_serverAddr.sin_port = htons(m_brokerPort);

    if (bind(
        m_sockFd,
        (struct sockaddr*)&m_serverAddr,
        sizeof(m_serverAddr)
    ) < 0) {
        spdlog::error("Failed to bind UDP socket");
        SocketAbstraction::SocketClose(m_sockFd);
        m_sockFd = -1;
        CleanupSockets();
        return MMW_ERROR;
    }

    spdlog::info("UDP broker listening on port {}", port);

    return MMW_OK;
}

MmwResult UdpTransport::Send(const std::string& data)
{
    int n = SocketAbstraction::SendTo(
        m_sockFd,
        data.data(),
        static_cast<int32_t>(data.size()),
        0,
        (struct sockaddr*)&m_serverAddr,
        sizeof(m_serverAddr)
    );

    if (n != static_cast<int>(data.size())) {
        return MMW_ERROR;
    }

    return MMW_OK;
}

MmwResult UdpTransport::Recv(std::string& data)
{
    char buffer[65536];

    sockaddr_in senderAddr;
    socklen_t senderLen = sizeof(senderAddr);

    int n = SocketAbstraction::RecvFrom(
        m_sockFd,
        buffer,
        sizeof(buffer),
        0,
        (struct sockaddr*)&senderAddr,
        &senderLen
    );

    if (n < 0) {
        return MMW_ERROR;
    }

    if (n == 0) {
        return MMW_DISCONNECTED;
    }

    data.assign(buffer, n);

    return MMW_OK;
}

MmwResult UdpTransport::Accept(
    std::atomic<bool>& running,
    ITransport*& client)
{
    (void)running;

    client = nullptr;

    // UDP is connectionless and has no accept().
    return MMW_ERROR;
}

void UdpTransport::Close()
{
    if (m_sockFd < 0) {
        return;
    }

    SocketAbstraction::SocketClose(m_sockFd);
    m_sockFd = -1;

    CleanupSockets();
}
