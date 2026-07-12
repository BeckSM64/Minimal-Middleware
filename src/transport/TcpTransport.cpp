#include "TcpTransport.h"

TcpTransport::TcpTransport()
    : m_socket(-1)
{
}

TcpTransport::TcpTransport(int socketFd)
    : m_socket(socketFd)
{
}

TcpTransport::~TcpTransport() {
    Close();
}

bool TcpTransport::Startup() {
    return SocketAbstraction::SocketStartup() == 0;
}

void TcpTransport::Cleanup() {
    SocketAbstraction::SocketCleanup();
}

bool TcpTransport::Connect(const std::string& host, unsigned short port) {
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0)
        return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (SocketAbstraction::InetPtonAbstraction(
            AF_INET, host.c_str(), &addr.sin_addr) != 1)
        return false;

    return connect(
               m_socket,
               reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) == 0;
}

bool TcpTransport::Bind(unsigned short port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int opt = 1;
    SocketAbstraction::SetSockOpt(
        m_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt),
        sizeof(opt));

    return bind(
               m_socket,
               reinterpret_cast<sockaddr*>(&addr),
               sizeof(addr)) == 0;
}

bool TcpTransport::Listen(int backlog) {
    return listen(m_socket, backlog) == 0;
}

int TcpTransport::Accept() {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);

    return accept(
        m_socket,
        reinterpret_cast<sockaddr*>(&addr),
        &len);
}

int TcpTransport::Send(const void* data, size_t size) {
    return SocketAbstraction::Send(
        m_socket,
        data,
        static_cast<int32_t>(size),
        0);
}

int TcpTransport::Recv(void* data, size_t size) {
    return SocketAbstraction::Recv(
        m_socket,
        data,
        static_cast<int32_t>(size),
        MSG_WAITALL);
}

void TcpTransport::Close() {
    if (m_socket != -1) {
        SocketAbstraction::SocketClose(m_socket);
        m_socket = -1;
    }
}

int TcpTransport::GetSocket() const {
    return m_socket;
}
