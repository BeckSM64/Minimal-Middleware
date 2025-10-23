#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <signal.h>
#include <sys/socket.h> // MSG_NOSIGNAL
#include <errno.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "MmwMessage.h"
#include "IMmwMessageSerializer.h"
#include "JsonSerializer.h"

#define PORT 5000

struct ConnectedClient {
    int socket_fd;
    std::string type; // "publisher" or "subscriber"
    std::string topic;
};

static std::vector<ConnectedClient> connectedClientList;
static std::mutex clientListMutex;

static std::vector<std::thread> clientThreads;
static std::mutex threadListMutex;

static int server_fd = -1;
static std::atomic<bool> running(true);

static IMmwMessageSerializer* g_serializer = nullptr;

// Send a length-prefixed message
inline bool sendMessage(int sock_fd, const std::string& data) {
    uint32_t len = htonl(data.size());
    if (send(sock_fd, &len, sizeof(len), 0) != sizeof(len)) return false;
    if (send(sock_fd, data.data(), data.size(), 0) != (ssize_t)data.size()) return false;
    return true;
}

// --- helper: safely route message to subscribers ---
void routeMessageToSubscribers(const std::string& topic, const MmwMessage& msg) {
    if (topic.empty()) return;

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
                std::remove_if(connectedClientList.begin(), connectedClientList.end(),
                               [fd](const ConnectedClient& c){ return c.socket_fd == fd; }),
                connectedClientList.end()
            );
            close(fd);
        }
    }
}

void removeClientByFd(int client_fd) {
    std::lock_guard<std::mutex> lock(clientListMutex);
    connectedClientList.erase(
        std::remove_if(connectedClientList.begin(), connectedClientList.end(),
                       [client_fd](const ConnectedClient& c){ return c.socket_fd == client_fd; }),
        connectedClientList.end()
    );
}

void handleClient(int client_fd) {
    while (running) {
        uint32_t netLen;
        ssize_t n = recv(client_fd, &netLen, sizeof(netLen), MSG_WAITALL);
        if (n <= 0) break;

        uint32_t msgLen = ntohl(netLen);
        if (msgLen == 0) continue;

        std::vector<char> buf(msgLen);
        n = recv(client_fd, buf.data(), msgLen, MSG_WAITALL);
        if (n <= 0) break;

        try {
            MmwMessage msg = g_serializer->deserialize(std::string(buf.data(), msgLen));

            if (msg.type == "register") {
                ConnectedClient newClient{client_fd, msg.payload, msg.topic};
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.push_back(newClient);
                spdlog::info("Registered {} for topic {} (fd={})", msg.payload, msg.topic, client_fd);
            } 
            else if (msg.type == "unregister") {
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.erase(
                    std::remove_if(connectedClientList.begin(), connectedClientList.end(),
                                   [&](const ConnectedClient& c){ return c.socket_fd == client_fd && c.topic == msg.topic; }),
                    connectedClientList.end()
                );
                spdlog::info("Unregistered client fd={} topic={}", client_fd, msg.topic);
            } 
            else if (msg.type == "publish") {
                routeMessageToSubscribers(msg.topic, msg);
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

    static JsonSerializer serializer;
    g_serializer = &serializer;

    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) { perror("bind"); return 1; }
    if (listen(server_fd, 16) < 0) { perror("listen"); return 1; }

    spdlog::info("Broker listening on port {}", PORT);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!running) break;
            if (errno == EINTR) continue;
            perror("accept"); continue;
        }

        spdlog::info("Client connected from {}:{} (fd={})", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);

        {
            std::lock_guard<std::mutex> lt(threadListMutex);
            clientThreads.emplace_back(std::thread(handleClient, client_fd));
        }
    }

    {
        std::lock_guard<std::mutex> lt(threadListMutex);
        for (auto &t : clientThreads) if (t.joinable()) t.join();
        clientThreads.clear();
    }

    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto &c : connectedClientList) if (c.socket_fd != -1) close(c.socket_fd);
        connectedClientList.clear();
    }

    if (server_fd != -1) { close(server_fd); server_fd = -1; }
    spdlog::info("Broker exited cleanly");
    return 0;
}
