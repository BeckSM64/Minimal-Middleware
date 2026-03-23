#ifdef __linux__ 
    #include <arpa/inet.h>
    #include <unistd.h>
#elif _WIN32

// Define target Windows version before including WinSock headers.
// Older MinGW-w64 builds don't expose modern network functions
// (like InetPtonA / InetPtonW) unless _WIN32_WINNT is set to at least 0x0600 (Vista+).
// This ensures ws2tcpip.h declares the required functions.
#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
#endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdint.h>
#else
    #error "Unsupported platform"
#endif

#include "SocketAbstraction.h"

int SocketAbstraction::SocketStartup() {

#ifdef __linux__ 
    // No startup required on Linux
#elif _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        // fprintf(stderr, "WSAStartup failed with error: %d\n", result);
        return result;
    }
#else
    #error "Unsupported platform"
#endif
    return 0;
}

int SocketAbstraction::SocketClose(int s) {
#ifdef __linux__
    shutdown(s, SHUT_RD);
    return close(s); // TODO: Might be helpful and more accurate to check the return of shutdown as well
#elif _WIN32
    shutdown(s, SD_RECEIVE);
    return closesocket(s);
#else
    #error "Unsupported platform"
#endif
}

int SocketAbstraction::SocketCleanup() {
#ifdef __linux__
    return 0;
#elif _WIN32
    return WSACleanup();
#else
    #error "Unsupported platform"
#endif
}

int SocketAbstraction::Send(int s, const void* buf, int32_t len, int32_t flags) {
    const char* ptr = (const char*)buf;
    int32_t totalSent = 0;

    while (totalSent < len) {
    #ifdef __linux__
        int n = send(s, ptr + totalSent, len - totalSent, flags | MSG_NOSIGNAL);
    #elif _WIN32
        int n = send(s, ptr + totalSent, len - totalSent, flags);
    #endif

        if (n <= 0) {
            return n; // error or disconnect
        }

        totalSent += n;
    }

    return totalSent;
}

int SocketAbstraction::Recv(int s, void* buf, int32_t len, int32_t flags) {
    char* ptr = (char*)buf;
    int32_t totalRecv = 0;

    while (totalRecv < len) {
        int n = recv(s, ptr + totalRecv, len - totalRecv, flags);
        if (n <= 0) {
            return n; // error or disconnect
        }
        totalRecv += n;
    }

    return totalRecv;
}

int SocketAbstraction::InetPtonAbstraction(int family, const char* pszAddrString, void* pAddrBuf) {
#ifdef __linux__
    return inet_pton(family, pszAddrString, pAddrBuf);
#elif _WIN32
    return InetPtonA(family, pszAddrString, pAddrBuf);
#else
    #error "Unsupported platform"
#endif
}

int SocketAbstraction::SetSockOpt(int s, int level, int optname, const char* optval, int optlen) {
    return setsockopt(s, level, optname, optval, optlen);
}
