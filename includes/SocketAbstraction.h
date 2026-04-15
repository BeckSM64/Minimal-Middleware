#ifndef SOCKET_ABSTRACTION_H
#define SOCKET_ABSTRACTION_H

#if defined(__linux__) || defined(__APPLE__)
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <stdint.h>
#elif defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdint.h>
    typedef int socklen_t;
#else
    #error "Unsupported platform"
#endif

class SocketAbstraction {
public:
    static int SocketStartup();
    static int SocketCleanup();
    static int SocketClose(int s);
    static int Send(int s, const void* buf, int32_t len, int32_t flags);
    static int Recv(int s, void* buf, int32_t len, int32_t flags);
    static int InetPtonAbstraction(int family, const char* pszAddrString, void* pAddrBuf);
    static int SetSockOpt(int s, int level, int optname, const char* optval, int optlen);
};

#endif