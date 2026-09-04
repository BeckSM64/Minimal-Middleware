#include "ITransport.h"
#include "SocketAbstraction.h"
#include "MMW.h"

class TcpTransport : public ITransport {
public:
    TcpTransport();
    ~TcpTransport();
    MmwResult Initialize() override;
    MmwResult Send(const std::string& data) override;
    MmwResult Recv(std::string& data) override;

private:
    int m_sockFd = -1;
    struct sockaddr_in m_serverAddr;
};