#include "EmscriptenTransport.h"

#include <cstdio>

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

EmscriptenTransport::EmscriptenTransport()
    : m_socket(0),
      m_connected(false),
      m_disconnected(false),
      m_brokerPort(0) {
}

EmscriptenTransport::~EmscriptenTransport() {
    Close();
}

MmwResult EmscriptenTransport::Initialize(
    std::string& hostname,
    int port
) {
    m_hostname = hostname;
    m_brokerPort = port;

    if (!emscripten_websocket_is_supported()) {
        return MMW_ERROR;
    }

    std::string url =
        "ws://" +
        m_hostname +
        ":" +
        std::to_string(m_brokerPort) +
        "/";

    EmscriptenWebSocketCreateAttributes attrs;

    attrs.url = url.c_str();
    attrs.protocols = NULL;
    attrs.createOnMainThread = EM_TRUE;

    m_socket = emscripten_websocket_new(&attrs);

    if (m_socket <= 0) {
        return MMW_ERROR;
    }

    if (emscripten_websocket_set_onopen_callback(
            m_socket,
            this,
            OnOpen) != EMSCRIPTEN_RESULT_SUCCESS) {
        Close();
        return MMW_ERROR;
    }

    if (emscripten_websocket_set_onmessage_callback(
            m_socket,
            this,
            OnMessage) != EMSCRIPTEN_RESULT_SUCCESS) {
        Close();
        return MMW_ERROR;
    }

    if (emscripten_websocket_set_onerror_callback(
            m_socket,
            this,
            OnError) != EMSCRIPTEN_RESULT_SUCCESS) {
        Close();
        return MMW_ERROR;
    }

    if (emscripten_websocket_set_onclose_callback(
            m_socket,
            this,
            OnClose) != EMSCRIPTEN_RESULT_SUCCESS) {
        Close();
        return MMW_ERROR;
    }

    /*
     * WebSocket connection is asynchronous.
     * Yield until the connection either opens or fails.
     */
    while (!m_connected && !m_disconnected) {
        emscripten_sleep(1);
    }

    if (!m_connected) {
        Close();
        return MMW_ERROR;
    }

    return MMW_OK;
}

MmwResult EmscriptenTransport::InitializeServer(int port) {
    /*
     * Browsers cannot listen for incoming TCP/WebSocket connections.
     */
    (void)port;
    return MMW_ERROR;
}

MmwResult EmscriptenTransport::Send(
    const std::string& data
) {
    if (!m_connected || m_socket <= 0) {
        return MMW_ERROR;
    }

    EMSCRIPTEN_RESULT result =
        emscripten_websocket_send_binary(
            m_socket,
            const_cast<char*>(data.data()),
            data.size()
        );

    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        return MMW_ERROR;
    }

    return MMW_OK;
}

MmwResult EmscriptenTransport::Recv(
    std::string& data
) {
    while (m_receivedMessages.empty() &&
           !m_disconnected) {
        emscripten_sleep(1);
    }

    if (!m_receivedMessages.empty()) {
        data = m_receivedMessages.front();
        m_receivedMessages.pop();
        return MMW_OK;
    }

    if (m_disconnected) {
        return MMW_DISCONNECTED;
    }

    return MMW_ERROR;
}

MmwResult EmscriptenTransport::Accept(
    std::atomic<bool>& running,
    ITransport*& client
) {
    (void)running;
    (void)client;

    /*
     * Browsers cannot act as a listening WebSocket server.
     */
    return MMW_ERROR;
}

void EmscriptenTransport::Close() {
    if (m_socket <= 0) {
        return;
    }

    emscripten_websocket_close(
        m_socket,
        1000,
        "Normal closure"
    );

    m_socket = 0;
    m_connected = false;
}

EM_BOOL EmscriptenTransport::OnOpen(
    int eventType,
    const EmscriptenWebSocketOpenEvent* event,
    void* userData
) {
    (void)eventType;
    (void)event;

    EmscriptenTransport* transport =
        static_cast<EmscriptenTransport*>(userData);

    transport->m_connected = true;
    transport->m_disconnected = false;

    return EM_TRUE;
}

EM_BOOL EmscriptenTransport::OnMessage(
    int eventType,
    const EmscriptenWebSocketMessageEvent* event,
    void* userData
) {
    (void)eventType;

    EmscriptenTransport* transport =
        static_cast<EmscriptenTransport*>(userData);

    if (event->isText) {
        return EM_TRUE;
    }

    transport->m_receivedMessages.push(
        std::string(
            reinterpret_cast<char*>(event->data),
            event->numBytes
        )
    );

    return EM_TRUE;
}

EM_BOOL EmscriptenTransport::OnError(
    int eventType,
    const EmscriptenWebSocketErrorEvent* event,
    void* userData
) {
    (void)eventType;
    (void)event;

    EmscriptenTransport* transport =
        static_cast<EmscriptenTransport*>(userData);

    transport->m_disconnected = true;

    return EM_TRUE;
}

EM_BOOL EmscriptenTransport::OnClose(
    int eventType,
    const EmscriptenWebSocketCloseEvent* event,
    void* userData
) {
    (void)eventType;
    (void)event;

    EmscriptenTransport* transport =
        static_cast<EmscriptenTransport*>(userData);

    transport->m_connected = false;
    transport->m_disconnected = true;

    return EM_TRUE;
}
