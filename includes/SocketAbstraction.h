#ifdef __linux__ 
    #include <arpa/inet.h>
#elif _WIN32
    #include <winsock2.h>
    #include <stdint.h>
    typedef int socklen_t;
#else
    #error "Unsupported platform"
#endif

class SocketAbstraction {

    public:
        static int SocketStartup();
        static int SocketCleanup();
        static int Send(int s, uint32_t* buf, int32_t len, int32_t flags);
        static int Recv(int s, uint32_t* buf, int32_t len, int32_t flags);
        static int InetPtonAbstraction(int family, const char* pszAddrString, PVOID pAddrBuf);
        static int SetSockOpt(int s, int level, int optname, const char* optval, int optlen);
};
