#pragma once

#include "ITransport.h"
#define ASIO_STANDALONE
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>

class WebSocketTransport : public ITransport {
public:
    typedef websocketpp::server<websocketpp::config::asio> Server;
    typedef websocketpp::connection_hdl Connection;

    WebSocketTransport(
        Server* server,
        Connection handle
    );

    ~WebSocketTransport();

    int Send(const void* data, size_t size) override;
    int Recv(void* buffer, size_t size) override;
    void Close() override;

    void OnMessage(const std::string& payload);

    bool Startup() override;
    void Cleanup() override;
    bool Connect(const std::string& host, unsigned short port) override;
    bool Bind(unsigned short port) override;
    bool Listen(int backlog) override;
    int Accept() override;
    int GetSocket() const override;

private:
    Server* m_server;
    Connection m_handle;

    std::queue<std::vector<char>> m_messages;
    std::mutex m_mutex;
    std::condition_variable m_cv;

    bool m_closed;
};