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
      m_acceptor(nullptr) {
}

BeastTransport::~BeastTransport() {
    Close();
}

MmwResult BeastTransport::Initialize() {
    try {
        asio::ip::tcp::resolver resolver(m_ioc);

        auto results = resolver.resolve(
            m_hostname,
            std::to_string(m_brokerPort)
        );

        asio::ip::tcp::socket socket(m_ioc);
        asio::connect(socket, results);

        m_ws = new websocket::stream<asio::ip::tcp::socket>(
            std::move(socket)
        );

        m_ws->handshake(m_hostname, "/");

        return MMW_OK;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to initialize Beast transport: {}", e.what());
        return MMW_ERROR;
    }
}

MmwResult BeastTransport::InitializeServer() {
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
        spdlog::error("Failed to initialize Beast server: {}", e.what());
        return MMW_ERROR;
    }
}

// MmwResult BeastTransport::Send(const std::string& data) {
//     try {
//         if (m_ws == nullptr) {
//             return MMW_ERROR;
//         }

//         m_ws->write(asio::buffer(data));

//         return MMW_OK;
//     }
//     catch (const std::exception& e) {
//         spdlog::error("Beast send failed: {}", e.what());
//         return MMW_ERROR;
//     }
// }

MmwResult BeastTransport::Send(const std::string& data) {
    try {
        if (m_ws == nullptr) {
            return MMW_ERROR;
        }

        m_ws->binary(true);
        m_ws->write(asio::buffer(data));

        return MMW_OK;
    }
    catch (const std::exception& e) {
        spdlog::error("Beast send failed: {}", e.what());
        return MMW_ERROR;
    }
}

MmwResult BeastTransport::Recv(std::string& data) {
    try {
        if (m_ws == nullptr) {
            return MMW_ERROR;
        }

        beast::flat_buffer buffer;

        m_ws->read(buffer);

        data = beast::buffers_to_string(buffer.data());

        return MMW_OK;
    }
    catch (const beast::system_error& e) {
        if (e.code() == websocket::error::closed) {
            return MMW_DISCONNECTED;
        }

        spdlog::error("Beast recv failed: {}", e.what());
        return MMW_ERROR;
    }
    catch (const std::exception& e) {
        spdlog::error("Beast recv failed: {}", e.what());
        return MMW_ERROR;
    }
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

        BeastTransport* clientTransport = new BeastTransport();

        clientTransport->m_ws =
            new websocket::stream<asio::ip::tcp::socket>(
                std::move(socket)
            );

        clientTransport->m_ws->accept();

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

void BeastTransport::Close() {
    if (m_ws != nullptr) {
        beast::error_code ec;

        m_ws->close(
            websocket::close_code::normal,
            ec
        );

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
