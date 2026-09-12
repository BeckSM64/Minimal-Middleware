#include <atomic>
#include "ITransport.h"
#include "SocketAbstraction.h"
#include "MMW.h"

class TcpTransport : public ITransport {
public:
    TcpTransport();
    TcpTransport(int sockFd);
    ~TcpTransport();
    MmwResult InitializeSockets();
    void CleanupSockets();
    MmwResult Initialize(std::string& hostname, int port) override;
    MmwResult InitializeServer(int port) override;
    MmwResult Send(const std::string& data) override;
    MmwResult Recv(std::string& data) override;
    MmwResult Accept(std::atomic<bool>& running, ITransport*& client) override;
    void Close() override;

private:
    int m_sockFd = -1;
    struct sockaddr_in m_serverAddr;
    static std::mutex s_socketMutex;
    static int s_socketUsers;
};