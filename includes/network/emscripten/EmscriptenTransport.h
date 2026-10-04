#ifndef EMSCRIPTEN_TRANSPORT_H
#define EMSCRIPTEN_TRANSPORT_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

#include <emscripten/websocket.h>

#include "ITransport.h"

class EmscriptenTransport : public ITransport {
public:
    EmscriptenTransport();
    ~EmscriptenTransport();

    MmwResult Initialize(std::string& hostname, int port) override;

    MmwResult InitializeServer(int port) override;

    MmwResult Send(const std::string& data) override;

    MmwResult Recv(std::string& data) override;

    MmwResult Accept(
        std::atomic<bool>& running,
        ITransport*& client
    ) override;

    void Close() override;

    void CloseWhenReady();

private:
    static EM_BOOL OnOpen(
        int eventType,
        const EmscriptenWebSocketOpenEvent* event,
        void* userData
    );

    static EM_BOOL OnMessage(
        int eventType,
        const EmscriptenWebSocketMessageEvent* event,
        void* userData
    );

    static EM_BOOL OnError(
        int eventType,
        const EmscriptenWebSocketErrorEvent* event,
        void* userData
    );

    static EM_BOOL OnClose(
        int eventType,
        const EmscriptenWebSocketCloseEvent* event,
        void* userData
    );

private:
    EMSCRIPTEN_WEBSOCKET_T m_socket;
    bool m_connected;
    bool m_disconnected;

    std::queue<std::string> m_receivedMessages;
    std::mutex m_receivedMessagesMutex;
    std::condition_variable m_receivedMessagesCondition;

    std::string m_hostname;
    int m_brokerPort;

    std::queue<std::string> m_pendingMessages;
    std::mutex m_pendingMessagesMutex;

    bool m_closeWhenConnected = false;
};

#endif