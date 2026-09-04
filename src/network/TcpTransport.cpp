#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "TcpTransport.h"
#include "SocketAbstraction.h"
#include "MMW.h"

TcpTransport::TcpTransport() {
    
}

TcpTransport::~TcpTransport() {

}

MmwResult TcpTransport::Initialize() {

    // Check return or socket call
    m_sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockFd == -1) {
        spdlog::error("Failed to create socket");
        return MMW_ERROR;
    }

    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(m_brokerPort);

    // Check return of inet_pton
    int rc = SocketAbstraction::InetPtonAbstraction(AF_INET, m_hostname.c_str(), &m_serverAddr.sin_addr);
    if (rc != 1) {
        spdlog::error("Invalid IP address provided: {}", m_hostname);
        SocketAbstraction::SocketClose(m_sockFd);
        return MMW_ERROR;
    }

    // Check return of connect
    if (connect(m_sockFd, (struct sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) < 0) {
        spdlog::error("Failed to connect to broker");
        SocketAbstraction::SocketClose(m_sockFd);
        return MMW_ERROR;
    }

    return MMW_OK;
}

MmwResult TcpTransport::Send(const std::string& data) {

    uint32_t len = htonl(data.size());

    if (SocketAbstraction::Send(m_sockFd, &len, sizeof(len), 0) != sizeof(len)) {
        return MMW_ERROR;
    }

    if (SocketAbstraction::Send(m_sockFd, data.data(), data.size(), 0) != (ssize_t)data.size()) {
        return MMW_ERROR;
    }

    return MMW_OK;
}

int TcpTransport::Recv() {
    return 0;
}
