#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "BeastTransport.h"
#include "MMW.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

BeastTransport::BeastTransport() {
}

BeastTransport::~BeastTransport() {
    Close();
}

MmwResult BeastTransport::Initialize(
    std::string& hostname,
    int port
) {
    m_hostname = hostname;
    m_brokerPort = port;

    try {
        asio::ip::tcp::resolver resolver(m_ioc);

        auto results = resolver.resolve(
            m_hostname,
            std::to_string(m_brokerPort)
        );

        asio::ip::tcp::socket socket(m_ioc);
        asio::connect(socket, results);

        m_ws = new WebSocket(std::move(socket));

        m_ws->handshake(m_hostname, "/");
        m_ws->binary(true);

        startIo();

        return MMW_OK;
    }
    catch (const std::exception& e) {
        spdlog::error(
            "Failed to initialize Beast transport: {}",
            e.what()
        );

        return MMW_ERROR;
    }
}

MmwResult BeastTransport::InitializeServer(int port) {
    m_brokerPort = port;

    try {
        m_acceptor = new asio::ip::tcp::acceptor(
            m_ioc,
            asio::ip::tcp::endpoint(
                asio::ip::tcp::v4(),
                m_brokerPort
            )
        );

        spdlog::info(
            "WebSocket broker listening on port {}",
            m_brokerPort
        );

        return MMW_OK;
    }
    catch (const std::exception& e) {
        spdlog::error(
            "Failed to initialize Beast server: {}",
            e.what()
        );

        return MMW_ERROR;
    }
}

void BeastTransport::startIo() {
    startRead();

    m_ioThread = std::thread([this]() {
        m_ioc.run();
    });
}

void BeastTransport::startRead() {
    if (m_ws == nullptr || m_closing) {
        return;
    }

    m_ws->async_read(
        m_readBuffer,
        [this](beast::error_code ec, std::size_t) {
            if (ec) {
                if (ec == websocket::error::closed) {
                    failReads(MMW_DISCONNECTED);
                }
                else {
                    spdlog::error(
                        "Beast recv failed: {}",
                        ec.message()
                    );

                    failReads(MMW_ERROR);
                }

                failWrites();
                return;
            }

            std::string data =
                beast::buffers_to_string(m_readBuffer.data());

            m_readBuffer.consume(m_readBuffer.size());

            {
                std::lock_guard<std::mutex> lock(m_receiveMutex);
                m_receiveQueue.push_back(data);
            }

            m_receiveCondition.notify_one();

            startRead();
        }
    );
}

MmwResult BeastTransport::Send(const std::string& data) {
    if (m_ws == nullptr || m_closing) {
        return MMW_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(m_writeMutex);

        if (m_closing) {
            return MMW_ERROR;
        }

        m_writeQueue.push_back(data);
    }

    asio::post(m_ioc, [this]() {
        startWrite();
    });

    return MMW_OK;
}

void BeastTransport::startWrite() {
    if (m_ws == nullptr || m_closing) {
        return;
    }

    std::string* data = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_writeMutex);

        if (m_writeInProgress || m_writeQueue.empty()) {
            return;
        }

        m_writeInProgress = true;
        data = &m_writeQueue.front();
    }

    m_ws->async_write(
        asio::buffer(*data),
        [this](beast::error_code ec, std::size_t) {
            {
                std::lock_guard<std::mutex> lock(m_writeMutex);

                if (!m_writeQueue.empty()) {
                    m_writeQueue.pop_front();
                }

                m_writeInProgress = false;
            }

            if (ec) {
                spdlog::error(
                    "Beast send failed: {}",
                    ec.message()
                );

                failWrites();
                failReads(MMW_ERROR);
                return;
            }

            startWrite();
        }
    );
}

MmwResult BeastTransport::Recv(std::string& data) {
    std::unique_lock<std::mutex> lock(m_receiveMutex);

    m_receiveCondition.wait(
        lock,
        [this]() {
            return !m_receiveQueue.empty() ||
                   m_receiveResult != MMW_OK ||
                   m_closing;
        }
    );

    if (!m_receiveQueue.empty()) {
        data = std::move(m_receiveQueue.front());
        m_receiveQueue.pop_front();
        return MMW_OK;
    }

    if (m_receiveResult != MMW_OK) {
        return m_receiveResult;
    }

    return MMW_DISCONNECTED;
}

MmwResult BeastTransport::Accept(
    std::atomic<bool>& running,
    ITransport*& client
) {
    try {
        if (m_acceptor == nullptr) {
            return MMW_ERROR;
        }

        /*
         * IMPORTANT:
         * The accepted socket belongs to the client's io_context,
         * not the server's io_context.
         */
        BeastTransport* clientTransport = new BeastTransport();

        asio::ip::tcp::socket socket(clientTransport->m_ioc);

        m_acceptor->accept(socket);

        clientTransport->m_ws =
            new WebSocket(std::move(socket));

        clientTransport->m_ws->accept();
        clientTransport->m_ws->binary(true);

        clientTransport->startIo();

        client = clientTransport;

        spdlog::info("WebSocket client connected");

        return MMW_OK;
    }
    catch (const std::exception& e) {
        if (!running) {
            return MMW_ERROR;
        }

        spdlog::error(
            "Failed to accept WebSocket client: {}",
            e.what()
        );

        return MMW_ERROR;
    }
}

void BeastTransport::failReads(MmwResult result) {
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);

        if (m_receiveResult == MMW_OK) {
            m_receiveResult = result;
        }
    }

    m_receiveCondition.notify_all();
}

void BeastTransport::failWrites() {
    std::lock_guard<std::mutex> lock(m_writeMutex);

    m_writeQueue.clear();
    m_writeInProgress = false;
}

void BeastTransport::Close() {
    if (m_closing.exchange(true)) {
        return;
    }

    failReads(MMW_DISCONNECTED);
    failWrites();

    m_receiveCondition.notify_all();

    if (m_ws != nullptr) {
        m_ioc.stop();
    }

    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }

    if (m_ws != nullptr) {
        delete m_ws;
        m_ws = nullptr;
    }

    if (m_acceptor != nullptr) {
        beast::error_code ec;
        m_acceptor->close(ec);

        delete m_acceptor;
        m_acceptor = nullptr;
    }
}
