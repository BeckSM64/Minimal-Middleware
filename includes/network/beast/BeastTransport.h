#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
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
    struct PendingWrite {
        std::string data;
        MmwResult result;
        bool done;
        std::condition_variable condition;

        PendingWrite()
            : result(MMW_ERROR),
              done(false) {
        }
    };

    void StartIoThread();
    void StartRead();
    void StartNextWrite();

    void HandleRead(
        const boost::system::error_code& ec,
        std::size_t bytesTransferred
    );

    void HandleWrite(
        std::shared_ptr<PendingWrite> write,
        const boost::system::error_code& ec,
        std::size_t bytesTransferred
    );

    void SetReceiveStatus(MmwResult result);
    void FailPendingWrites();

private:
    boost::asio::io_context m_ioc;

    boost::beast::websocket::stream<
        boost::asio::ip::tcp::socket
    >* m_ws;

    boost::asio::ip::tcp::acceptor* m_acceptor;

    boost::beast::flat_buffer m_readBuffer;

    std::thread m_ioThread;

    std::mutex m_receiveMutex;
    std::condition_variable m_receiveCondition;
    std::deque<std::string> m_receivedMessages;
    MmwResult m_receiveStatus;

    std::mutex m_writeMutex;
    std::deque<std::shared_ptr<PendingWrite> > m_writeQueue;
    bool m_writeInProgress;

    std::atomic<bool> m_closing;
    std::atomic<bool> m_ioStarted;
};
