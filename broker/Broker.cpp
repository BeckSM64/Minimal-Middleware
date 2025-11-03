#include <unistd.h>
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

#define PORT 5000

struct ConnectedClient {
    int socket_fd;
    std::string type; // "publisher" or "subscriber"
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

static int server_fd = -1;
static std::atomic<bool> running(true);

static IMmwMessageSerializer* g_serializer = nullptr;

static std::mutex ackMutex;
static std::unordered_map<int, std::unordered_map<uint32_t, PendingAck>> unackedMessages;

static std::atomic<uint32_t> brokerMessageId{1}; // start at 1

// Send a length-prefixed message
inline bool sendMessage(int sock_fd, const std::string& data) {

    SocketAbstraction::SocketStartup();

    uint32_t len = htonl(data.size());
    if (SocketAbstraction::Send(sock_fd, &len, sizeof(len), 0) != sizeof(len)) {
        return false;
    }
    if (SocketAbstraction::Send(sock_fd, (uint32_t*) data.data(), data.size(), 0) != (ssize_t)data.size()) {
        return false;
    }
    return true;
}

// --- helper: safely route message to subscribers ---
void routeMessageToSubscribers(const std::string& topic, const MmwMessage& msg) {
    if (topic.empty()) {
        return;
    }

    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto& client : connectedClientList) {
            if (client.type == "subscriber" && client.topic == topic) {
                targets.push_back(client.socket_fd);
            }
        }
    }

    std::string serialized = g_serializer->serialize(msg);

    for (int fd : targets) {
        if (!sendMessage(fd, serialized)) {
            spdlog::error("send to subscriber fd={} failed, removing client", fd);
            std::lock_guard<std::mutex> lock(clientListMutex);
            connectedClientList.erase(
                std::remove_if(
                    connectedClientList.begin(), connectedClientList.end(),
                        [fd](const ConnectedClient& c){
                        return c.socket_fd == fd;
                    }
                ),
                connectedClientList.end()
            );
            close(fd);
        
        // Only track unacked messages if reliability was set
        } else if (msg.reliability) {
            std::lock_guard<std::mutex> lock(ackMutex);
            PendingAck ack;
            ack.msg = msg;
            ack.timestamp = std::chrono::steady_clock::now();
            ack.retryCount = 0;
            unackedMessages[fd][msg.messageId] = ack;

        }
    }
}

void removeClientByFd(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        connectedClientList.erase(
            std::remove_if(
                connectedClientList.begin(), connectedClientList.end(),
                [client_fd](const ConnectedClient& c){
                    return c.socket_fd == client_fd;
                }
            ),
            connectedClientList.end()
        );
    }

    // Remove unacked messages when subscriber disconnects
    {
        std::lock_guard<std::mutex> lock(ackMutex);
        unackedMessages.erase(client_fd);
    }
}


void handleClient(int client_fd) {
    while (running) {
        uint32_t netLen;
        ssize_t n = SocketAbstraction::Recv(client_fd, &netLen, sizeof(netLen), MSG_WAITALL);
        if (n <= 0) {
            break;
        }

        uint32_t msgLen = ntohl(netLen);
        if (msgLen == 0) {
            continue;
        }

        std::vector<char> buf(msgLen);
        n = SocketAbstraction::Recv(client_fd, (uint32_t*) buf.data(), msgLen, MSG_WAITALL);
        if (n <= 0) {
            break;
        }

        try {
            MmwMessage msg = g_serializer->deserialize(std::string(buf.data(), msgLen));

            if (msg.type == "register") {
                auto now = std::chrono::steady_clock::now();
                ConnectedClient newClient{client_fd, msg.payload, msg.topic, std::chrono::steady_clock::now()};
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.push_back(newClient);
                spdlog::info("Registered {} for topic {} (fd={})", msg.payload, msg.topic, client_fd);
            } else if (msg.type == "unregister") {
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.erase(
                    std::remove_if(
                        connectedClientList.begin(), connectedClientList.end(),
                            [&](const ConnectedClient& c){
                            return c.socket_fd == client_fd && c.topic == msg.topic;
                        }
                    ),
                    connectedClientList.end()
                );
                spdlog::info("Unregistered client fd={} topic={}", client_fd, msg.topic);
            } else if (msg.type == "publish") {

                // Assign a unique messageId
                // TODO: This could eventually reach a limit
                msg.messageId = brokerMessageId++;
                routeMessageToSubscribers(msg.topic, msg);

            } else if (msg.type == "ack") {
                std::lock_guard<std::mutex> lock(ackMutex);
                auto subIt = unackedMessages.find(client_fd);
                if (subIt != unackedMessages.end()) {
                    subIt->second.erase(msg.messageId);
                }
                spdlog::info("Received ACK for message {} from subscriber fd={}", msg.messageId, client_fd);
            } else if (msg.type == "heartbeat") {
                std::lock_guard<std::mutex> lock(clientListMutex);
                for (auto& client : connectedClientList) {
                    if (client.socket_fd == client_fd) {
                        client.lastHeartbeat = std::chrono::steady_clock::now();
                        break;
                    }
                }
                spdlog::info("Received heartbeat for message  subscriber fd={}", client_fd);
            }

        } catch (const std::exception& e) {
            spdlog::error("Failed to deserialize message: {}", e.what());
        }
    }

    close(client_fd);
    removeClientByFd(client_fd);
    spdlog::info("Client disconnected (fd={})", client_fd);
}

void handleSignal(int signum) {
    spdlog::info("Signal received ({}), shutting down broker...", signum);
    running = false;
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
}

int main() {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    g_serializer = CreateSerializer();

    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    SocketAbstraction::SocketStartup();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, 16) < 0) {
        perror("listen");
        return 1;
    }

    spdlog::info("Broker listening on port {}", PORT);

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
                    spdlog::warn("Subscriber fd={} timed out, removing", it->socket_fd);
                    close(it->socket_fd);
                    it = connectedClientList.erase(it);
                } else {
                    ++it;
                }
            }
        }
    });

    // Start resend thread for unacked messages
    constexpr int MAX_RETRIES = 3;
    std::thread resendThread([]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto now = std::chrono::steady_clock::now();
            std::vector<int> fdsToRemove;

            {
                std::lock_guard<std::mutex> lock(ackMutex);
                for (auto& clientPair : unackedMessages) {
                    int fd = clientPair.first;
                    auto& msgMap = clientPair.second;

                    for (auto it = msgMap.begin(); it != msgMap.end();) {
                        auto& pending = it->second;
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - pending.timestamp);

                        if (elapsed.count() > 2) { // retry delay
                            if (pending.retryCount >= MAX_RETRIES) {
                                spdlog::error("Max retries reached for message {} to fd={}", pending.msg.messageId, fd);
                                fdsToRemove.push_back(fd);
                                break;
                            } else {
                                spdlog::warn("Resending message {} to fd={}", pending.msg.messageId, fd);
                                sendMessage(fd, g_serializer->serialize(pending.msg));
                                pending.timestamp = now;
                                pending.retryCount++;
                                ++it;
                            }
                        } else {
                            ++it;
                        }
                    }
                }

                for (int fd : fdsToRemove) {
                    unackedMessages.erase(fd);
                    close(fd);
                    removeClientByFd(fd);
                }
            }
        }
    });

    // Accept loop
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!running) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        spdlog::info("Client connected from {}:{} (fd={})", inet_ntoa(client_addr.sin_addr),
                     ntohs(client_addr.sin_port), client_fd);

        {
            std::lock_guard<std::mutex> lt(threadListMutex);
            clientThreads.emplace_back(std::thread(handleClient, client_fd));
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
            if (c.socket_fd != -1) {
                close(c.socket_fd);
            }
        }
        connectedClientList.clear();
    }

    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
    spdlog::info("Broker exited cleanly");

    SocketAbstraction::SocketCleanup();
    return 0;
}
