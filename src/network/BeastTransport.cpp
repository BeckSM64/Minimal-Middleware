#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "BeastTransport.h"
#include "MMW.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

BeastTransport::BeastTransport()
    : m_ws(nullptr),
      m_acceptor(nullptr),
      m_receiveStatus(MMW_OK),
      m_writeInProgress(false),
      m_closing(false),
      m_ioStarted(false) {
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

        m_ws = new websocket::stream<
            asio::ip::tcp::socket
        >(std::move(socket));

        m_ws->handshake(m_hostname, "/");
        m_ws->binary(true);

        StartIoThread();

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

MmwResult BeastTransport::Send(const std::string& data) {
    if (m_ws == nullptr || m_closing) {
        return MMW_ERROR;
    }

    std::shared_ptr<PendingWrite> write =
        std::make_shared<PendingWrite>();

    write->data = data;

    {
        std::lock_guard<std::mutex> lock(m_writeMutex);
        m_writeQueue.push_back(write);
    }

    asio::post(
        m_ioc,
        [this]() {
            StartNextWrite();
        }
    );

    std::unique_lock<std::mutex> lock(m_writeMutex);

    write->condition.wait(
        lock,
        [&write]() {
            return write->done;
        }
    );

    return write->result;
}

MmwResult BeastTransport::Recv(std::string& data) {
    std::unique_lock<std::mutex> lock(m_receiveMutex);

    m_receiveCondition.wait(
        lock,
        [this]() {
            return
                !m_receivedMessages.empty() ||
                m_receiveStatus != MMW_OK ||
                m_closing;
        }
    );

    if (!m_receivedMessages.empty()) {
        data = m_receivedMessages.front();
        m_receivedMessages.pop_front();
        return MMW_OK;
    }

    if (m_receiveStatus != MMW_OK) {
        return m_receiveStatus;
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

        asio::ip::tcp::socket socket(m_ioc);

        m_acceptor->accept(socket);

        BeastTransport* clientTransport =
            new BeastTransport();

        clientTransport->m_ws =
            new websocket::stream<
                asio::ip::tcp::socket
            >(std::move(socket));

        clientTransport->m_ws->accept();
        clientTransport->m_ws->binary(true);

        clientTransport->StartIoThread();

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

void BeastTransport::StartIoThread() {
    if (m_ioStarted) {
        return;
    }

    m_ioStarted = true;

    m_ioc.post(
        [this]() {
            StartRead();
        }
    );

    m_ioThread = std::thread(
        [this]() {
            m_ioc.run();
        }
    );
}

void BeastTransport::StartRead() {
    if (m_ws == nullptr || m_closing) {
        return;
    }

    m_ws->async_read(
        m_readBuffer,
        [this](
            const boost::system::error_code& ec,
            std::size_t bytesTransferred
        ) {
            HandleRead(ec, bytesTransferred);
        }
    );
}

void BeastTransport::HandleRead(
    const boost::system::error_code& ec,
    std::size_t
) {
    if (ec) {
        if (ec == websocket::error::closed) {
            SetReceiveStatus(MMW_DISCONNECTED);
        }
        else if (!m_closing) {
            spdlog::error(
                "Beast recv failed: {}",
                ec.message()
            );

            SetReceiveStatus(MMW_ERROR);
        }

        FailPendingWrites();
        return;
    }

    std::string data =
        beast::buffers_to_string(m_readBuffer.data());

    m_readBuffer.consume(m_readBuffer.size());

    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);
        m_receivedMessages.push_back(data);
    }

    m_receiveCondition.notify_one();

    StartRead();
}

void BeastTransport::StartNextWrite() {
    if (m_ws == nullptr || m_closing) {
        FailPendingWrites();
        return;
    }

    std::shared_ptr<PendingWrite> write;

    {
        std::lock_guard<std::mutex> lock(m_writeMutex);

        if (m_writeInProgress ||
            m_writeQueue.empty()) {
            return;
        }

        write = m_writeQueue.front();
        m_writeInProgress = true;
    }

    m_ws->async_write(
        asio::buffer(write->data),
        [this, write](
            const boost::system::error_code& ec,
            std::size_t bytesTransferred
        ) {
            HandleWrite(
                write,
                ec,
                bytesTransferred
            );
        }
    );
}

void BeastTransport::HandleWrite(
    std::shared_ptr<PendingWrite> write,
    const boost::system::error_code& ec,
    std::size_t
) {
    {
        std::lock_guard<std::mutex> lock(m_writeMutex);

        if (ec) {
            write->result = MMW_ERROR;
        }
        else {
            write->result = MMW_OK;
        }

        write->done = true;

        if (!m_writeQueue.empty() &&
            m_writeQueue.front() == write) {
            m_writeQueue.pop_front();
        }

        m_writeInProgress = false;
    }

    write->condition.notify_one();

    if (ec) {
        if (!m_closing) {
            spdlog::error(
                "Beast send failed: {}",
                ec.message()
            );

            SetReceiveStatus(MMW_ERROR);
        }

        FailPendingWrites();
        return;
    }

    StartNextWrite();
}

void BeastTransport::SetReceiveStatus(MmwResult result) {
    {
        std::lock_guard<std::mutex> lock(m_receiveMutex);

        if (m_receiveStatus == MMW_OK) {
            m_receiveStatus = result;
        }
    }

    m_receiveCondition.notify_all();
}

void BeastTransport::FailPendingWrites() {
    std::deque<std::shared_ptr<PendingWrite> > pending;

    {
        std::lock_guard<std::mutex> lock(m_writeMutex);

        pending.swap(m_writeQueue);
        m_writeInProgress = false;

        for (
            std::deque<std::shared_ptr<PendingWrite> >::iterator it =
                pending.begin();
            it != pending.end();
            ++it
        ) {
            (*it)->result = MMW_ERROR;
            (*it)->done = true;
        }
    }

    for (
        std::deque<std::shared_ptr<PendingWrite> >::iterator it =
            pending.begin();
        it != pending.end();
        ++it
    ) {
        (*it)->condition.notify_one();
    }
}

void BeastTransport::Close() {
    if (m_closing.exchange(true)) {
        return;
    }

    m_receiveCondition.notify_all();

    FailPendingWrites();

    if (m_ioStarted) {
        m_ioc.stop();

        if (m_ioThread.joinable()) {
            m_ioThread.join();
        }
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
