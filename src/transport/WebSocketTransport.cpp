#define ASIO_STANDALONE
#include "WebSocketTransport.h"

WebSocketTransport::WebSocketTransport(
    Server* server,
    Connection handle
)
    :
    m_server(server),
    m_handle(handle),
    m_closed(false)
{
}

WebSocketTransport::~WebSocketTransport()
{
    Close();
}


int WebSocketTransport::Send(
    const void* data,
    size_t size
)
{
    if (m_closed)
        return -1;

    try {
        m_server->send(
            m_handle,
            data,
            size,
            websocketpp::frame::opcode::binary
        );

        return static_cast<int>(size);
    }
    catch (...) {
        return -1;
    }
}


int WebSocketTransport::Recv(
    void* buffer,
    size_t size
)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_cv.wait(lock, [this]() {
        return !m_messages.empty() || m_closed;
    });

    if (m_messages.empty())
        return -1;


    auto& msg = m_messages.front();

    size_t copySize = std::min(
        size,
        msg.size()
    );

    memcpy(
        buffer,
        msg.data(),
        copySize
    );

    m_messages.pop();

    return static_cast<int>(copySize);
}


void WebSocketTransport::OnMessage(
    const std::string& payload
)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_messages.push(
            std::vector<char>(
                payload.begin(),
                payload.end()
            )
        );
    }

    m_cv.notify_one();
}


void WebSocketTransport::Close()
{
    if (m_closed)
        return;

    m_closed = true;

    m_cv.notify_all();

    try {
        m_server->close(
            m_handle,
            websocketpp::close::status::normal,
            ""
        );
    }
    catch (...) {
    }
}

bool WebSocketTransport::Startup()
{
    return true;
}

void WebSocketTransport::Cleanup()
{
}

bool WebSocketTransport::Connect(const std::string&, unsigned short)
{
    return false; // listener creates websocket connections
}

bool WebSocketTransport::Bind(unsigned short)
{
    return false;
}

bool WebSocketTransport::Listen(int)
{
    return false;
}

int WebSocketTransport::Accept()
{
    return -1;
}

int WebSocketTransport::GetSocket() const
{
    return -1; // no socket exposed
}