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
#include <memory>
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
#include "TcpListener.h"
#include "IListener.h"

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

struct ConnectedClient {
    std::shared_ptr<ITransport> transport;
    std::string type;
    std::string topic;
    std::chrono::steady_clock::time_point lastHeartbeat;
};

struct PendingAck {
    MmwMessage msg;
    std::chrono::steady_clock::time_point timestamp;
    int retryCount = 0;  // count how many times we've resent
};

static std::vector<ConnectedClient> connectedClientList;
static std::mutex clientListMutex;

static std::vector<std::thread> clientThreads;
static std::mutex threadListMutex;

static std::unique_ptr<IListener> listener;
static std::atomic<bool> running(true);

static IMmwMessageSerializer* g_serializer = nullptr;

static std::mutex ackMutex;
static std::unordered_map<ITransport*, std::unordered_map<uint32_t, PendingAck>> unackedMessages;

static std::atomic<uint32_t> brokerMessageId{1}; // start at 1

static BrokerPersistence* g_persistence = nullptr;

std::map<ITransport*, std::mutex> transportSendMutexes;
static std::mutex socketSendMutexMapLock;

// Send a length-prefixed message
inline bool sendMessage(ITransport& transport, const std::string& data)
{
    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> lock(socketSendMutexMapLock);
        mtx = &transportSendMutexes[&transport];
    }

    std::lock_guard<std::mutex> lock(*mtx);

    uint32_t len = htonl(data.size());

    if (transport.Send(&len, sizeof(len)) != sizeof(len))
        return false;

    if (transport.Send(data.data(), data.size()) != (int)data.size())
        return false;

    return true;
}

// Helper function to route messages to subscribers
void routeMessageToSubscribers(const std::string& topic, const MmwMessage& msg)
{
    if (topic.empty()) {
        return;
    }

    std::vector<std::shared_ptr<ITransport>> targets;
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto& client : connectedClientList) {
            if (client.type == "subscriber" && client.topic == topic) {
                targets.push_back(client.transport);
            }
        }
    }

    std::string serialized = g_serializer->serialize(msg);

    for (auto& transport : targets) {
        if (!sendMessage(*transport, serialized)) {
            spdlog::error("Failed to send to subscriber, removing client");

            std::lock_guard<std::mutex> lock(clientListMutex);
            connectedClientList.erase(
                std::remove_if(
                    connectedClientList.begin(),
                    connectedClientList.end(),
                    [&](const ConnectedClient& c) {
                        return c.transport == transport;
                    }),
                connectedClientList.end());

            transport->Close();

        } else if (msg.reliability) {
            std::lock_guard<std::mutex> lock(ackMutex);

            PendingAck ack;
            ack.msg = msg;
            ack.timestamp = std::chrono::steady_clock::now();
            ack.retryCount = 0;

            unackedMessages[transport.get()][msg.messageId] = ack;
        }
    }
}

void removeClient(ITransport* transport)
{
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        connectedClientList.erase(
            std::remove_if(
                connectedClientList.begin(),
                connectedClientList.end(),
                [transport](const ConnectedClient& c) {
                    return c.transport.get() == transport;
                }),
            connectedClientList.end());
    }

    // Remove unacked messages when subscriber disconnects
    {
        std::lock_guard<std::mutex> lock(ackMutex);
        unackedMessages.erase(transport);
    }
}


void handleClient(std::shared_ptr<ITransport> transport)
{
    while (running) {
        uint32_t netLen;
        int n = transport->Recv(&netLen, sizeof(netLen));
        if (n <= 0) {
            break;
        }

        uint32_t msgLen = ntohl(netLen);
        if (msgLen == 0) {
            continue;
        }

        std::vector<char> buf(msgLen);
        n = transport->Recv(buf.data(), msgLen);
        if (n <= 0) {
            break;
        }

        try {
            MmwMessage msg = g_serializer->deserialize(std::string(buf.data(), msgLen));

            if (msg.type == "register") {
                ConnectedClient newClient{
                    transport,
                    msg.payload,
                    msg.topic,
                    std::chrono::steady_clock::now()
                };

                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.push_back(newClient);

                spdlog::info("Registered {} for topic {}", msg.payload, msg.topic);

            } else if (msg.type == "unregister") {

                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.erase(
                    std::remove_if(
                        connectedClientList.begin(),
                        connectedClientList.end(),
                        [&](const ConnectedClient& c) {
                            return c.transport == transport &&
                                   c.topic == msg.topic;
                        }),
                    connectedClientList.end());

                spdlog::info("Unregistered client topic={}", msg.topic);

            } else if (msg.type == "publish") {

                msg.messageId = brokerMessageId++;

                if (!g_persistence->persistMessage(msg)) {
                    spdlog::warn("Failed to persist message {}", msg.messageId);
                }

                routeMessageToSubscribers(msg.topic, msg);

            } else if (msg.type == "ack") {

                std::lock_guard<std::mutex> lock(ackMutex);
                auto subIt = unackedMessages.find(transport.get());
                if (subIt != unackedMessages.end()) {
                    subIt->second.erase(msg.messageId);
                }

                spdlog::info("Received ACK for message {}", msg.messageId);

            } else if (msg.type == "heartbeat") {

                std::lock_guard<std::mutex> lock(clientListMutex);
                for (auto& client : connectedClientList) {
                    if (client.transport == transport) {
                        client.lastHeartbeat = std::chrono::steady_clock::now();
                        break;
                    }
                }

                spdlog::info("Received heartbeat");
            }

        } catch (const std::exception& e) {
            spdlog::error("Failed to deserialize message: {}", e.what());
        }
    }

    transport->Close();
    removeClient(transport.get());
    spdlog::info("Client disconnected");
}

void handleSignal(int signum) {
    spdlog::info("Signal received ({}), shutting down broker...", signum);
    running = false;

    if (listener) {
        listener->Close();
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    g_serializer = CreateSerializer();
    g_persistence = new BrokerPersistence("broker_data.db");

    brokerMessageId = g_persistence->getNextMessageId();

    SocketAbstraction::SocketStartup();

    int port = 5000;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port <= 0 || port > 65535) {
                spdlog::warn("Invalid port '{}', using default {}", argv[1], port);
                port = 5000;
            }
        } catch (...) {
            spdlog::warn("Invalid port '{}', using default {}", argv[1], port);
            port = 5000;
        }
    }

    listener.reset(new TcpListener());

    if (!listener->Listen(port)) {
        spdlog::error("Failed to start listener on port {}", port);
        return 1;
    }

    spdlog::info("Broker listening on port {}", port);


    std::thread heartbeatMonitor([]() {
        constexpr int TIMEOUT_MS = 6000;

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            auto now = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(clientListMutex);

            for (auto it = connectedClientList.begin();
                 it != connectedClientList.end();) {

                if (it->type == "subscriber" &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - it->lastHeartbeat).count() > TIMEOUT_MS) {

                    spdlog::warn("Subscriber timed out, removing");

                    it->transport->Close();
                    it = connectedClientList.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
    });


    constexpr int MAX_RETRIES = 3;

    std::thread resendThread([MAX_RETRIES]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto now = std::chrono::steady_clock::now();

            std::vector<ITransport*> transportsToRemove;

            {
                std::lock_guard<std::mutex> lock(ackMutex);

                for (auto& clientPair : unackedMessages) {
                    auto transport = clientPair.first;
                    auto& msgMap = clientPair.second;

                    for (auto it = msgMap.begin();
                         it != msgMap.end();) {

                        auto& pending = it->second;

                        auto elapsed =
                            std::chrono::duration_cast<std::chrono::seconds>(
                                now - pending.timestamp);

                        if (elapsed.count() > 2) {

                            if (pending.retryCount >= MAX_RETRIES) {
                                spdlog::error(
                                    "Max retries reached for message {}",
                                    pending.msg.messageId);

                                transportsToRemove.push_back(transport);
                                break;
                            }
                            else {
                                spdlog::warn(
                                    "Resending message {}",
                                    pending.msg.messageId);

                                sendMessage(
                                    *transport,
                                    g_serializer->serialize(pending.msg));

                                pending.timestamp = now;
                                pending.retryCount++;
                                ++it;
                            }
                        }
                        else {
                            ++it;
                        }
                    }
                }

                for (auto& transport : transportsToRemove) {
                    unackedMessages.erase(transport);
                    transport->Close();
                    removeClient(transport);
                }
            }
        }
    });


    // Accept loop
    while (running) {

        auto transport = listener->Accept();

        if (!transport) {
            if (!running)
                break;

            spdlog::warn("Failed to accept client");
            continue;
        }

        spdlog::info("Client connected");

        std::lock_guard<std::mutex> lt(threadListMutex);

        clientThreads.emplace_back(
            std::thread(handleClient, transport)
        );
    }


    listener->Close();


    heartbeatMonitor.join();

    {
        std::lock_guard<std::mutex> lt(threadListMutex);

        for (auto& t : clientThreads) {
            if (t.joinable())
                t.join();
        }

        clientThreads.clear();
    }


    resendThread.join();


    {
        std::lock_guard<std::mutex> lock(clientListMutex);

        for (auto& c : connectedClientList) {
            if (c.transport) {
                c.transport->Close();
            }
        }

        connectedClientList.clear();
    }


    delete g_persistence;
    g_persistence = nullptr;

    delete g_serializer;
    g_serializer = nullptr;

    SocketAbstraction::SocketCleanup();

    spdlog::info("Broker exited cleanly");

    return 0;
}
