#include "TcpListener.h"

#include "TcpTransport.h"
#include "SocketAbstraction.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
    #include <arpa/inet.h>
    #include <netinet/in.h>
#elif defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif

TcpListener::TcpListener()
    : m_listenSocket(-1)
{
}

TcpListener::~TcpListener()
{
    Close();
}

bool TcpListener::Listen(uint16_t port)
{
    if (SocketAbstraction::SocketStartup() != 0)
        return false;

    m_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenSocket < 0)
        return false;

    int opt = 1;
    SocketAbstraction::SetSockOpt(
        m_listenSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt),
        sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(
            m_listenSocket,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)) != 0)
    {
        Close();
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) != 0)
    {
        Close();
        return false;
    }

    return true;
}

std::shared_ptr<ITransport> TcpListener::Accept()
{
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);

    int clientSocket = accept(
        m_listenSocket,
        reinterpret_cast<sockaddr*>(&addr),
        &len);

    if (clientSocket < 0)
        return nullptr;

    return std::make_shared<TcpTransport>(clientSocket);
}

void TcpListener::Close()
{
    if (m_listenSocket != -1)
    {
        SocketAbstraction::SocketClose(m_listenSocket);
        m_listenSocket = -1;
    }

    SocketAbstraction::SocketCleanup();
}
