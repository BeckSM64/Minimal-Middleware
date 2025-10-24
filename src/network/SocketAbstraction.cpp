#ifdef __linux__ 
    #include <arpa/inet.h>
#elif _WIN32
    #include <winsock2.h>
    #include <stdint.h>
#else
    #error "Unsupported platform"
#endif

#include <iostream>

#include "SocketAbstraction.h"

int SocketAbstraction::SocketStartup() {

#ifdef __linux__ 
    // No startup required on Linux
#elif _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", result);
        return result;
    }
#else
    #error "Unsupported platform"
#endif
    return 0;
}


int SocketAbstraction::SocketCleanup() {
    return WSACleanup();
}

int SocketAbstraction::Send(int s, uint32_t* buf, int32_t len, int32_t flags) {
    return send(s, (const char*) buf, len, flags);
}

int SocketAbstraction::Recv(int s, uint32_t* buf, int32_t len, int32_t flags) {
    return recv(s, (char*) buf, len, flags);
}

INT SocketAbstraction::InetPtonAbstraction(INT family, const char* pszAddrString, PVOID pAddrBuf) {
    return InetPtonA(family, pszAddrString, pAddrBuf);
}
