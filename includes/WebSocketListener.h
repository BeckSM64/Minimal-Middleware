#pragma once

#include "IListener.h"
#define ASIO_STANDALONE
#include "WebSocketTransport.h"

#include <queue>
#include <mutex>
#include <condition_variable>


class WebSocketListener : public IListener {
public:
    WebSocketListener();
    ~WebSocketListener();

    bool Listen(uint16_t port) override;

    std::shared_ptr<ITransport> Accept() override;

    void Close() override;


private:
    typedef websocketpp::server<websocketpp::config::asio> Server;

    Server m_server;

    std::queue<std::shared_ptr<WebSocketTransport>> m_clients;

    std::mutex m_mutex;
    std::condition_variable m_cv;

    bool m_running;
};