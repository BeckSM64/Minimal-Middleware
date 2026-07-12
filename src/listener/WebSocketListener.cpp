#define ASIO_STANDALONE
#include "WebSocketListener.h"

WebSocketListener::WebSocketListener()
    :
    m_running(false)
{
}


WebSocketListener::~WebSocketListener()
{
    Close();
}


bool WebSocketListener::Listen(uint16_t port)
{
    try {

        m_server.init_asio();

        m_server.set_open_handler(
            [this](websocketpp::connection_hdl hdl)
            {
                auto transport =
                    std::make_shared<WebSocketTransport>(
                        &m_server,
                        hdl
                    );

                {
                    std::lock_guard<std::mutex> lock(m_mutex);

                    m_clients.push(
                        transport
                    );
                }

                m_cv.notify_one();
            }
        );


        m_server.listen(port);

        m_server.start_accept();

        m_running = true;


        std::thread(
            [this]()
            {
                m_server.run();
            }
        ).detach();


        return true;
    }
    catch (...) {
        return false;
    }
}


std::shared_ptr<ITransport> WebSocketListener::Accept()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_cv.wait(lock, [this]()
    {
        return !m_clients.empty() || !m_running;
    });


    if (m_clients.empty())
        return nullptr;


    auto client = m_clients.front();

    m_clients.pop();

    return client;
}


void WebSocketListener::Close()
{
    if (!m_running)
        return;

    m_running = false;

    try {
        m_server.stop_listening();
        m_server.stop();
    }
    catch (...) {
    }

    m_cv.notify_all();
}