#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <signal.h>
#include <errno.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <cxxopts.hpp>

#include "MmwMessage.h"
#include "IMmwMessageSerializer.h"
#include "SerializerAbstraction.h"
#include "BrokerPersistence.h"
#include "ITransport.h"
#include "TcpTransport.h"
#include "BeastTransport.h"

struct ConnectedTransportClient {
    ITransport* transport;
    std::string type; // "publisher" or "subscriber"
    std::string topic;
    std::chrono::steady_clock::time_point lastHeartbeat;
};

struct PendingAck {
    MmwMessage msg;
    std::chrono::steady_clock::time_point timestamp;
    int retryCount = 0;  // count how many times we've resent
};

static std::vector<ConnectedTransportClient> connectedClientList;
static std::mutex clientListMutex;
static std::vector<std::thread> clientThreads;
static std::mutex threadListMutex;
static std::atomic<bool> running(true);
static IMmwMessageSerializer* g_serializer = nullptr;
static std::mutex ackMutex;
static std::unordered_map<ITransport*, std::unordered_map<uint32_t, PendingAck>> unackedMessages;
static std::atomic<uint32_t> brokerMessageId{1}; // start at 1
static BrokerPersistence* g_persistence = nullptr;
static std::map<ITransport*, std::mutex> transportSendMutexes;
static std::mutex transportSendMutexMapLock;
static ITransport* serverTcpTransport = nullptr;
static ITransport* serverBeastTransport = nullptr;

inline bool sendMessage(ITransport* transport, const std::string& data) {
    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> lock(transportSendMutexMapLock);
        mtx = &transportSendMutexes[transport];
    }

    std::lock_guard<std::mutex> lock(*mtx);

    return transport->Send(data) == MMW_OK;
}

// Helper function to route messages to subscribers
void routeMessageToSubscribers(const std::string& topic, const MmwMessage& msg) {
    if (topic.empty()) {
        return;
    }

    std::vector<ITransport*> targets;
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto& client : connectedClientList) {
            if (client.type == "subscriber" && client.topic == topic) {
                targets.push_back(client.transport);
            }
        }
    }

    std::string serialized = g_serializer->serialize(msg);

    for (ITransport* transport : targets) {

        // Only track unacked messages if reliability was set
        if (msg.reliability) {
            std::lock_guard<std::mutex> lock(ackMutex);
            PendingAck ack;
            ack.msg = msg;
            ack.timestamp = std::chrono::steady_clock::now();
            ack.retryCount = 0;
            unackedMessages[transport][msg.messageId] = ack;

        }

        if (!sendMessage(transport, serialized)) {
            spdlog::error(
                "Failed to route {} message {} to subscriber",
                msg.topic,
                msg.messageId
            );
            std::lock_guard<std::mutex> lock(clientListMutex);
            connectedClientList.erase(
                std::remove_if(
                    connectedClientList.begin(), connectedClientList.end(),
                        [transport](const ConnectedTransportClient& c){
                        return c.transport == transport;
                    }
                ),
                connectedClientList.end()
            );
            transport->Close();
        }
    }
}

void removeClientByTransport(ITransport* transport) {
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        connectedClientList.erase(
            std::remove_if(
                connectedClientList.begin(), connectedClientList.end(),
                [transport](const ConnectedTransportClient& c){
                    return c.transport == transport;
                }
            ),
            connectedClientList.end()
        );
    }

    // Remove unacked messages when subscriber disconnects
    {
        std::lock_guard<std::mutex> lock(ackMutex);
        unackedMessages.erase(transport);
    }
}

void handleClient(ITransport* transport) {
    while (running) {

        std::string data;
        MmwResult result = transport->Recv(data);

        if (result == MMW_DISCONNECTED) {
            break;
        }

        if (result == MMW_ERROR) {
            spdlog::error("Recv for message failed...");
            break;
        }

        try {
            MmwMessage msg = g_serializer->deserialize(data);

            if (msg.type == "register") {
                auto now = std::chrono::steady_clock::now();
                ConnectedTransportClient newClient{transport, msg.payload, msg.topic, std::chrono::steady_clock::now()};
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.push_back(newClient);
                spdlog::info("Registered {} for topic {}", msg.payload, msg.topic);
            } else if (msg.type == "unregister") {
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.erase(
                    std::remove_if(
                        connectedClientList.begin(), connectedClientList.end(),
                            [&](const ConnectedTransportClient& c){
                            return c.transport == transport && c.topic == msg.topic;
                        }
                    ),
                    connectedClientList.end()
                );
                spdlog::info("Unregistered topic={}", msg.topic);
            } else if (msg.type == "publish") {

                // Assign a unique messageId
                // TODO: This could eventually reach a limit
                msg.messageId = brokerMessageId++;

                // Write message to sqlite database for persistence
                if (!g_persistence->persistMessage(msg)) {
                    spdlog::warn("Failed to persist message {}", msg.messageId);
                }

                routeMessageToSubscribers(msg.topic, msg);

            } else if (msg.type == "ack") {
                std::lock_guard<std::mutex> lock(ackMutex);
                auto subIt = unackedMessages.find(transport);
                if (subIt != unackedMessages.end()) {
                    subIt->second.erase(msg.messageId);
                }
                spdlog::info("Received ACK for message {} from subscriber", msg.messageId);
            } else if (msg.type == "heartbeat") {
                std::lock_guard<std::mutex> lock(clientListMutex);
                for (auto& client : connectedClientList) {
                    if (client.transport == transport) {
                        client.lastHeartbeat = std::chrono::steady_clock::now();
                        break;
                    }
                }
                spdlog::info("Received heartbeat for message  subscriber");
            }

        } catch (const std::exception& e) {
            spdlog::error("Failed to deserialize message: {}", e.what());
        }
    }

    transport->Close();
    removeClientByTransport(transport);
    spdlog::info("Client disconnected");
}

void handleSignal(int signum) {
    spdlog::info("Signal received ({}), shutting down broker...", signum);

    running = false;

    if (serverTcpTransport != nullptr) {
        serverTcpTransport->Close();
    }

    if (serverBeastTransport != nullptr) {
        serverBeastTransport->Close();
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    spdlog::set_level(spdlog::level::err);

    // Set the cxx options for argument parsing
    cxxopts::Options options("mmw_broker", "MMW Broker");
    options.add_options()
        ("tcp-port", "TCP port", cxxopts::value<int>()->default_value("5000"))
        ("ws-port", "WebSocket port", cxxopts::value<int>()->default_value("5001"))
        ("h,help", "Show help");

    // Attempt to parse the arguments
    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);

        if (result.count("help")) {
            spdlog::info(options.help());
            return 0;
        }
    } catch (const cxxopts::exceptions::exception& e) {
        spdlog::error("{}", e.what());
        return 1;
    }

    // Get the tcp and websocket ports from the cxxopts parse result
    int tcpPort = result["tcp-port"].as<int>();
    int wsPort = result["ws-port"].as<int>();

    // Check that the ports are in a valid range
    if (tcpPort <= 0 || tcpPort > 65535) {
        spdlog::warn("Invalid port number '{}', using default {}", tcpPort, 5000);
        tcpPort = 5000;
    }

    if (wsPort <= 0 || wsPort > 65535) {
        spdlog::warn("Invalid port number '{}', using default {}", wsPort, 5001);
        wsPort = 5001;
    }

    // Create the global serializer and initialize the db for message persistence
    g_serializer = CreateSerializer();
    g_persistence = new BrokerPersistence("broker_data.db");

    // Initialize brokerMessageId based on existing messages in DB
    brokerMessageId = g_persistence->getNextMessageId();

    // Initialize the tcp and beast transports
    serverTcpTransport = new TcpTransport();
    serverBeastTransport = new BeastTransport();

    serverTcpTransport->InitializeServer(tcpPort);
    serverBeastTransport->InitializeServer(wsPort);

    // Start heartbeat monitoring thread
    std::thread heartbeatMonitor([]() {
        constexpr int TIMEOUT_MS = 6000; // 6 seconds timeout
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            auto now = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(clientListMutex);
            for (auto it = connectedClientList.begin(); it != connectedClientList.end();) {
                if (it->type == "subscriber" &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - it->lastHeartbeat).count() > TIMEOUT_MS) {
                    spdlog::warn("Subscriber fd={} timed out, removing");
                    it->transport->Close();
                    it = connectedClientList.erase(it);
                } else {
                    ++it;
                }
            }
        }
    });

    // Start resend thread for unacked messages
    // TODO: These retries should probably be configurable
    constexpr int MAX_RETRIES = 3;

    std::thread resendThread([MAX_RETRIES]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto now = std::chrono::steady_clock::now();

            struct Retry {
                ITransport* transport;
                std::string data;
                uint64_t messageId;
            };

            std::vector<Retry> retries;
            std::vector<ITransport*> transportsToRemove;

            // Inspect/update unacked messages while holding the mutex
            {
                std::lock_guard<std::mutex> lock(ackMutex);

                for (auto& clientPair : unackedMessages) {
                    ITransport* transport = spdlog::set_level(spdlog::level::err);clientPair.first;
                    auto& msgMap = clientPair.second;

                    for (auto& msgPair : msgMap) {
                        auto& pending = msgPair.second;

                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - pending.timestamp);

                        if (elapsed.count() > 2) {
                            if (pending.retryCount >= MAX_RETRIES) {
                                transportsToRemove.push_back(transport);
                                break;
                            }

                            retries.push_back({
                                transport,
                                g_serializer->serialize(pending.msg),
                                pending.msg.messageId
                            });

                            pending.timestamp = now;
                            pending.retryCount++;
                        }
                    }
                }
            }

            // Do network I/O WITHOUT holding ackMutex.
            for (auto& retry : retries) {
                spdlog::warn("Resending message {}", retry.messageId);
                sendMessage(retry.transport, retry.data);
            }

            // Remove failed transports WITHOUT holding ackMutex.
            for (ITransport* transport : transportsToRemove) {
                {
                    std::lock_guard<std::mutex> lock(ackMutex);
                    unackedMessages.erase(transport);
                }

                transport->Close();
                removeClientByTransport(transport);
            }
        }
    });

    std::thread tcpAcceptThread([&]() {
        while (running) {
            ITransport* clientTransport = nullptr;

            if (serverTcpTransport->Accept(running, clientTransport) == MMW_OK) {
                std::lock_guard<std::mutex> lt(threadListMutex);
                clientThreads.emplace_back(handleClient, clientTransport);
            }
        }
    });

    std::thread beastAcceptThread([&]() {
        while (running) {
            ITransport* clientTransport = nullptr;

            if (serverBeastTransport->Accept(running, clientTransport) == MMW_OK) {
                std::lock_guard<std::mutex> lt(threadListMutex);
                clientThreads.emplace_back(handleClient, clientTransport);
            }
        }
    });

    // These will get cleaned up by the OS when the process exits
    tcpAcceptThread.detach();
    beastAcceptThread.detach();

    // Join all threads
    heartbeatMonitor.join();

    {
        std::lock_guard<std::mutex> lt(threadListMutex);
        for (auto& t : clientThreads) {
            if (t.joinable()) {
                t.join();
            }
        }
        clientThreads.clear();
    }

    resendThread.join();

    // Cleanup remaining clients
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto& c : connectedClientList) {
            if (c.transport != nullptr) {
                c.transport->Close();
            }
        }
        connectedClientList.clear();
    }

    if (serverTcpTransport != nullptr) {
        serverTcpTransport->Close();
    }

    if (serverBeastTransport != nullptr) {
        serverBeastTransport->Close();
    }

    // Cleanup broker persistence
    delete g_persistence;
    g_persistence = nullptr;

    // Cleanup serializer
    delete g_serializer;
    g_serializer = nullptr;

    spdlog::info("Broker exited cleanly");

    return 0;
}
