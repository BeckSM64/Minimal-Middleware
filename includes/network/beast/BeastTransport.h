#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "ITransport.h"

class BeastTransport : public ITransport {
public:
    BeastTransport();
    ~BeastTransport();

    MmwResult Initialize(std::string& hostname, int port) override;
    MmwResult InitializeServer(int port) override;

    MmwResult Send(const std::string& data) override;
    MmwResult Recv(std::string& data) override;

    MmwResult Accept(
        std::atomic<bool>& running,
        ITransport*& client
    ) override;

    void Close() override;

private:
    boost::asio::io_context m_ioc;

    std::shared_ptr<
        boost::beast::websocket::stream<
            boost::asio::ip::tcp::socket
        >
    > m_ws;

    std::shared_ptr<
        boost::asio::ip::tcp::acceptor
    > m_acceptor;
};
