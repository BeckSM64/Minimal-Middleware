#include "ITransport.h"

class TcpTransport : public ITransport {
    TcpTransport();
    ~TcpTransport();
    void Initialize() override;
    int Send() override;
    int Recv() override;
};