#include "EmscriptenTransport.h"

#include <cstdio>

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

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
    spdlog::info(
        "EmscriptenTransport::Initialize({}, {})",
        hostname,
        port
    );

    m_hostname = hostname;
    m_brokerPort = port;

    if (!emscripten_websocket_is_supported()) {
        spdlog::error("WebSocket is not supported");
        return MMW_ERROR;
    }

    spdlog::info("WebSocket is supported");

    std::string url =
        "ws://" +
        m_hostname +
        ":" +
        std::to_string(m_brokerPort) +
        "/";

    spdlog::info("Creating WebSocket: {}", url);

    EmscriptenWebSocketCreateAttributes attrs;

    attrs.url = url.c_str();
    attrs.protocols = NULL;
    attrs.createOnMainThread = EM_TRUE;

    m_socket = emscripten_websocket_new(&attrs);

    if (m_socket <= 0) {
        spdlog::error(
            "emscripten_websocket_new failed: socket={}",
            m_socket
        );
        return MMW_ERROR;
    }

    spdlog::info("WebSocket created: socket={}", m_socket);

    EMSCRIPTEN_RESULT result;

    result = emscripten_websocket_set_onopen_callback(
        m_socket,
        this,
        OnOpen
    );

    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        spdlog::error(
            "Failed to set onopen callback: result={}",
            result
        );
        Close();
        return MMW_ERROR;
    }

    spdlog::info("onopen callback registered");

    result = emscripten_websocket_set_onmessage_callback(
        m_socket,
        this,
        OnMessage
    );

    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        spdlog::error(
            "Failed to set onmessage callback: result={}",
            result
        );
        Close();
        return MMW_ERROR;
    }

    spdlog::info("onmessage callback registered");

    result = emscripten_websocket_set_onerror_callback(
        m_socket,
        this,
        OnError
    );

    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        spdlog::error(
            "Failed to set onerror callback: result={}",
            result
        );
        Close();
        return MMW_ERROR;
    }

    spdlog::info("onerror callback registered");

    result = emscripten_websocket_set_onclose_callback(
        m_socket,
        this,
        OnClose
    );

    if (result != EMSCRIPTEN_RESULT_SUCCESS) {
        spdlog::error(
            "Failed to set onclose callback: result={}",
            result
        );
        Close();
        return MMW_ERROR;
    }

    spdlog::info("onclose callback registered");
    spdlog::info("EmscriptenTransport::Initialize succeeded");

    return MMW_OK;
}

MmwResult EmscriptenTransport::InitializeServer(int port) {
    /*
     * Browsers cannot listen for incoming TCP/WebSocket connections.
     */
    (void)port;
    return MMW_ERROR;
}

MmwResult EmscriptenTransport::Send(const std::string& data) {
    if (m_socket <= 0 || m_disconnected) {
        return MMW_ERROR;
    }

    if (!m_connected) {
        std::lock_guard<std::mutex> lock(m_pendingMessagesMutex);
        m_pendingMessages.push(data);
        return MMW_OK;
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
    std::unique_lock<std::mutex> lock(m_receivedMessagesMutex);

    m_receivedMessagesCondition.wait(
        lock,
        [this]() {
            return !m_receivedMessages.empty() || m_disconnected;
        }
    );

    if (m_receivedMessages.empty()) {
        return MMW_DISCONNECTED;
    }

    data = m_receivedMessages.front();
    m_receivedMessages.pop();

    return MMW_OK;
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
    m_disconnected = true;

    m_receivedMessagesCondition.notify_all();
}

EM_BOOL EmscriptenTransport::OnOpen(
    int eventType,
    const EmscriptenWebSocketOpenEvent* websocketEvent,
    void* userData
) {
    (void)eventType;
    (void)websocketEvent;

    EmscriptenTransport* transport =
        static_cast<EmscriptenTransport*>(userData);

    transport->m_connected = true;
    transport->m_disconnected = false;

    spdlog::info("Emscripten WebSocket connected");

    std::lock_guard<std::mutex> lock(
        transport->m_pendingMessagesMutex
    );

    while (!transport->m_pendingMessages.empty()) {
        const std::string& data =
            transport->m_pendingMessages.front();

        EMSCRIPTEN_RESULT result =
            emscripten_websocket_send_binary(
                transport->m_socket,
                const_cast<char*>(data.data()),
                data.size()
            );

        if (result != EMSCRIPTEN_RESULT_SUCCESS) {
            spdlog::error(
                "Failed to send queued WebSocket message"
            );
            break;
        }

        transport->m_pendingMessages.pop();
    }

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

    {
        std::lock_guard<std::mutex> lock(
            transport->m_receivedMessagesMutex
        );

        transport->m_receivedMessages.push(
            std::string(
                reinterpret_cast<char*>(event->data),
                event->numBytes
            )
        );
    }

    transport->m_receivedMessagesCondition.notify_one();

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
    transport->m_receivedMessagesCondition.notify_all();

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

    transport->m_receivedMessagesCondition.notify_all();

    return EM_TRUE;
}