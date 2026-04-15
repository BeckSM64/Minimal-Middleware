#include "SocketAbstraction.h"

int SocketAbstraction::SocketStartup() {
#if defined(_WIN32)
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) return result;
#endif
    return 0;
}

int SocketAbstraction::SocketClose(int s) {
#if defined(_WIN32)
    shutdown(s, SD_RECEIVE);
    return closesocket(s);
#else
    shutdown(s, SHUT_RD);
    return close(s);
#endif
}

int SocketAbstraction::SocketCleanup() {
#if defined(_WIN32)
    return WSACleanup();
#endif
    return 0;
}

int SocketAbstraction::Send(int s, const void* buf, int32_t len, int32_t flags) {
    const char* ptr = (const char*)buf;
    int32_t totalSent = 0;

    // Handle macOS SIGPIPE prevention at the socket level if not already set
#if defined(__APPLE__)
    int set = 1;
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif

    while (totalSent < len) {
        int n;
    #if defined(__linux__)
        n = send(s, ptr + totalSent, len - totalSent, flags | MSG_NOSIGNAL);
    #else
        // Windows and macOS use standard flags here
        n = send(s, ptr + totalSent, len - totalSent, flags);
    #endif

        if (n <= 0) return n;
        totalSent += n;
    }
    return totalSent;
}

int SocketAbstraction::Recv(int s, void* buf, int32_t len, int32_t flags) {
    char* ptr = (char*)buf;
    int32_t totalRecv = 0;

    while (totalRecv < len) {
        int n = recv(s, ptr + totalRecv, len - totalRecv, flags);
        if (n <= 0) return n;
        totalRecv += n;
    }
    return totalRecv;
}

int SocketAbstraction::InetPtonAbstraction(int family, const char* pszAddrString, void* pAddrBuf) {
#if defined(_WIN32)
    return InetPtonA(family, pszAddrString, pAddrBuf);
#else
    return inet_pton(family, pszAddrString, pAddrBuf);
#endif
}

int SocketAbstraction::SetSockOpt(int s, int level, int optname, const char* optval, int optlen) {
    // Cast to (const char*) for Windows compatibility, (const void*) for POSIX
    return setsockopt(s, level, optname, (const char*)optval, optlen);
}
