#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

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
    typedef boost::asio::ip::tcp::socket TcpSocket;
    typedef boost::beast::websocket::stream<TcpSocket> WebSocket;

    void startIo();
    void startRead();
    void startWrite();

    void failReads(MmwResult result);
    void failWrites();

    boost::asio::io_context m_ioc;

    boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type
    > m_workGuard;

    WebSocket* m_ws = nullptr;
    boost::asio::ip::tcp::acceptor* m_acceptor = nullptr;

    std::thread m_ioThread;

    std::mutex m_receiveMutex;
    std::condition_variable m_receiveCondition;
    std::deque<std::string> m_receiveQueue;
    MmwResult m_receiveResult = MMW_OK;

    std::mutex m_writeMutex;
    std::deque<std::string> m_writeQueue;
    bool m_writeInProgress = false;

    std::atomic<bool> m_closing{false};

    boost::beast::flat_buffer m_readBuffer;
};
