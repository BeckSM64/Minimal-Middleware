#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "TcpTransport.h"
#include "SocketAbstraction.h"
#include "MMW.h"

TcpTransport::TcpTransport() {
    
}

TcpTransport::TcpTransport(int sockFd) : m_sockFd(sockFd) {
    
}

TcpTransport::~TcpTransport() {
    SocketAbstraction::SocketClose(m_sockFd);
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

MmwResult TcpTransport::InitializeServer() {

    socklen_t addrlen = sizeof(m_serverAddr);

    SocketAbstraction::SocketStartup();

    m_sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockFd == -1) {
        spdlog::error("Failed to create socket");
        return MMW_ERROR;
    }

    // TODO: Move to initialize with isServer check
    int opt = 1;
    setsockopt(m_sockFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // TODO: Hardcode to 5000 for now
    int port = 5000;
    // if (argc > 1) {
    //     try {
    //         port = std::stoi(argv[1]);
    //         if (port <= 0 || port > 65535) {
    //             spdlog::warn("Invalid port number '{}', using default {}", argv[1], port);
    //             port = 5000;
    //         }
    //     } catch (const std::exception& e) {
    //         spdlog::warn("Invalid port argument '{}', using default {}", argv[1], port);
    //         port = 5000;
    //     }
    // }

    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_addr.s_addr = INADDR_ANY;
    m_serverAddr.sin_port = htons(port);

    if (bind(m_sockFd, (struct sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) < 0) {
        spdlog::error("Failed to bind");
        return MMW_ERROR;
    }
    if (listen(m_sockFd, 16) < 0) {
        spdlog::error("Failed to listen");
        return MMW_ERROR;
    }

    spdlog::info("Broker listening on port {}", port);

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

MmwResult TcpTransport::Recv(std::string& data) {
    uint32_t netLen;

    int n = SocketAbstraction::Recv(
        m_sockFd,
        &netLen,
        sizeof(netLen),
        MSG_WAITALL
    );

    if (n == 0) {
        return MMW_DISCONNECTED;
    }

    if (n < 0) {
        return MMW_ERROR;
    }

    uint32_t msgLen = ntohl(netLen);

    if (msgLen > 1024 * 1024) {
        spdlog::error("Received message too large: {} bytes", msgLen);
        return MMW_ERROR;
    }

    if (msgLen == 0) {
        data.clear();
        return MMW_OK;
    }

    std::vector<char> buf(msgLen);

    n = SocketAbstraction::Recv(m_sockFd, buf.data(), msgLen, MSG_WAITALL);

    if (n == 0) {
        return MMW_DISCONNECTED;  // if you have such a result
    }

    if (n < 0) {
        return MMW_ERROR;
    }

    data.assign(buf.data(), msgLen);

    return MMW_OK;
}

MmwResult TcpTransport::Accept(std::atomic<bool>& running, ITransport*& client) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(
        m_sockFd,
        (struct sockaddr*)&client_addr,
        &client_len
    );

    if (client_fd < 0) {
        if (!running)
            return MMW_ERROR;

        if (errno == EINTR)
            return MMW_ERROR;

        spdlog::error("Failed to accept");
        return MMW_ERROR;
    }

    spdlog::info(
        "Client connected from {}:{} (fd={})",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port),
        client_fd
    );

    TcpTransport* clientTransport = new TcpTransport();
    clientTransport->m_sockFd = client_fd;

    client = clientTransport;

    return MMW_OK;
}

void TcpTransport::Close() {
    if (m_sockFd != -1) {
        SocketAbstraction::SocketClose(m_sockFd);
        m_sockFd = -1;
    }
}
