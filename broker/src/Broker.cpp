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

#include "MmwMessage.h"
#include "IMmwMessageSerializer.h"
#include "SerializerAbstraction.h"
#include "SocketAbstraction.h"
#include "BrokerPersistence.h"
#include "ITransport.h"
#include "TcpTransport.h"

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

struct ConnectedClient {
    int socket_fd;
    std::string type; // "publisher" or "subscriber"
    std::string topic;
    std::chrono::steady_clock::time_point lastHeartbeat;
};

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
// static std::unordered_map<int, std::unordered_map<uint32_t, PendingAck>> unackedMessages;
static std::unordered_map<ITransport*, std::unordered_map<uint32_t, PendingAck>> unackedMessages;

static std::atomic<uint32_t> brokerMessageId{1}; // start at 1

static BrokerPersistence* g_persistence = nullptr;

static std::map<int, std::mutex> socketSendMutexes;
static std::mutex socketSendMutexMapLock;

static std::map<ITransport*, std::mutex> transportSendMutexes;
static std::mutex transportSendMutexMapLock;

static ITransport* serverTransport = nullptr;

// Send a length-prefixed message
inline bool sendMessage(int sock_fd, const std::string& data) {
    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> lock(socketSendMutexMapLock);
        mtx = &socketSendMutexes[sock_fd];
    }

    std::lock_guard<std::mutex> lock(*mtx);

    uint32_t len = htonl(data.size());

    if (SocketAbstraction::Send(sock_fd, &len, sizeof(len), 0) != sizeof(len)) {
        return false;
    }

    if (SocketAbstraction::Send(sock_fd, data.data(), data.size(), 0) != (ssize_t)data.size()) {
        return false;
    }

    return true;
}

inline bool sendMessage(ITransport* transport, const std::string& data) {
    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> lock(socketSendMutexMapLock);
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
        if (!sendMessage(transport, serialized)) {
            // spdlog::error("send to subscriber fd={} failed, removing client", fd);
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
            // SocketAbstraction::SocketClose(fd);
        
        // Only track unacked messages if reliability was set
        } else if (msg.reliability) {
            std::lock_guard<std::mutex> lock(ackMutex);
            PendingAck ack;
            ack.msg = msg;
            ack.timestamp = std::chrono::steady_clock::now();
            ack.retryCount = 0;
            unackedMessages[transport][msg.messageId] = ack;

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
            spdlog::error("FAILING TO RECEIVE");
            break;
        }

        try {
            MmwMessage msg = g_serializer->deserialize(data);

            if (msg.type == "register") {
                auto now = std::chrono::steady_clock::now();
                ConnectedTransportClient newClient{transport, msg.payload, msg.topic, std::chrono::steady_clock::now()};
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.push_back(newClient);
                // spdlog::info("Registered {} for topic {} (fd={})", msg.payload, msg.topic, client_fd);
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
                // spdlog::info("Unregistered client fd={} topic={}", client_fd, msg.topic);
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
                // spdlog::info("Received ACK for message {} from subscriber fd={}", msg.messageId, client_fd);
            } else if (msg.type == "heartbeat") {
                std::lock_guard<std::mutex> lock(clientListMutex);
                for (auto& client : connectedClientList) {
                    if (client.transport == transport) {
                        client.lastHeartbeat = std::chrono::steady_clock::now();
                        break;
                    }
                }
                // spdlog::info("Received heartbeat for message  subscriber fd={}", client_fd);
            }

        } catch (const std::exception& e) {
            spdlog::error("Failed to deserialize message: {}", e.what());
        }
    }

    // SocketAbstraction::SocketClose(client_fd);
    // removeClientByFd(client_fd);
    removeClientByTransport(transport);
    // spdlog::info("Client disconnected (fd={})", client_fd);
}

void handleSignal(int signum) {
    spdlog::info("Signal received ({}), shutting down broker...", signum);

    running = false;

    if (serverTransport != nullptr) {
        serverTransport->Close();
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    g_serializer = CreateSerializer();
    g_persistence = new BrokerPersistence("broker_data.db");

    // Initialize brokerMessageId based on existing messages in DB
    brokerMessageId = g_persistence->getNextMessageId();

    // struct sockaddr_in address;
    // socklen_t addrlen = sizeof(address);

    // SocketAbstraction::SocketStartup();

    // server_fd = socket(AF_INET, SOCK_STREAM, 0);
    // if (server_fd == -1) {
    //     spdlog::error("Failed to create socket");
    //     return -1;
    // }

    // // TODO: Move to initialize with isServer check
    // int opt = 1;
    // setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // int port = 5000;
    // if (argc > 1) {
    //     try {
    //         port = std::stoi(argv[1]);
    //         if (port <= 0 || port > 65535) {
    //             spdlog::warn("Invalid port number '{}', using default {}", argv[1], port);
    //             port = 5000;
    //         }
    //     } catch (const std::exception& e) {
    //         spdlog::warn("Invalid port argument '{}', using default {}", argv[1], port);
    //         port = 5000;
    //     }
    // }

    // address.sin_family = AF_INET;
    // address.sin_addr.s_addr = INADDR_ANY;
    // address.sin_port = htons(port);

    // if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    //     spdlog::error("Failed to bind");
    //     return -1;
    // }
    // if (listen(server_fd, 16) < 0) {
    //     spdlog::error("Failed to listen");
    //     return -1;
    // }

    serverTransport = new TcpTransport();
    serverTransport->InitializeServer();

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
                    // spdlog::warn("Subscriber fd={} timed out, removing", it->socket_fd);
                    spdlog::warn("Subscriber fd={} timed out, removing");
                    // SocketAbstraction::SocketClose(it->socket_fd);
                    it = connectedClientList.erase(it);
                } else {
                    ++it;
                }
            }
        }
    });

    // Start resend thread for unacked messages
    constexpr int MAX_RETRIES = 3;
    std::thread resendThread([MAX_RETRIES]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto now = std::chrono::steady_clock::now();
            // std::vector<int> fdsToRemove;
            std::vector<ITransport*> transportsToRemove;

            {
                std::lock_guard<std::mutex> lock(ackMutex);
                for (auto& clientPair : unackedMessages) {
                    // int fd = clientPair.first;
                    ITransport* transport = clientPair.first;
                    auto& msgMap = clientPair.second;

                    for (auto it = msgMap.begin(); it != msgMap.end();) {
                        auto& pending = it->second;
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - pending.timestamp);

                        if (elapsed.count() > 2) { // retry delay
                            if (pending.retryCount >= MAX_RETRIES) {
                                // spdlog::error("Max retries reached for message {} to fd={}", pending.msg.messageId, fd);
                                // fdsToRemove.push_back(fd);
                                transportsToRemove.push_back(transport);
                                break;
                            } else {
                                // spdlog::warn("Resending message {} to fd={}", pending.msg.messageId, fd);
                                // sendMessage(fd, g_serializer->serialize(pending.msg));
                                sendMessage(transport, g_serializer->serialize(pending.msg));
                                pending.timestamp = now;
                                pending.retryCount++;
                                ++it;
                            }
                        } else {
                            ++it;
                        }
                    }
                }

                // for (int fd : fdsToRemove) {
                //     unackedMessages.erase(fd);
                //     SocketAbstraction::SocketClose(fd);
                //     removeClientByFd(fd);
                // }

                for (ITransport* transport : transportsToRemove) {
                    unackedMessages.erase(transport);
                    // SocketAbstraction::SocketClose(fd);
                    // removeClientByFd(fd);
                    removeClientByTransport(transport);
                }
            }
        }
    });

    // Accept loop
    while (running) {
        // struct sockaddr_in client_addr;
        // socklen_t client_len = sizeof(client_addr);
        // int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        // if (client_fd < 0) {
        //     if (!running) {
        //         break;
        //     }
        //     if (errno == EINTR) {
        //         continue;
        //     }
        //     spdlog::error("Failed to accept");
        //     continue;
        // }

        // spdlog::info("Client connected from {}:{} (fd={})", inet_ntoa(client_addr.sin_addr),
        //              ntohs(client_addr.sin_port), client_fd);

        ITransport* clientTransport = nullptr;
        if (serverTransport->Accept(running, clientTransport) == MMW_ERROR) {
            if (!running)
                break;

            continue;
        }

        {
            std::lock_guard<std::mutex> lt(threadListMutex);
            clientThreads.emplace_back(std::thread(handleClient, clientTransport));
        }
    }

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
                // SocketAbstraction::SocketClose(c.socket_fd);
            }
        }
        connectedClientList.clear();
    }

    if (serverTransport != nullptr) {
        serverTransport->Close();
    }

    // Cleanup broker persistence
    delete g_persistence;
    g_persistence = nullptr;

    // Cleanup serializer
    delete g_serializer;
    g_serializer = nullptr;

    SocketAbstraction::SocketCleanup();
    spdlog::info("Broker exited cleanly");

    return 0;
}
