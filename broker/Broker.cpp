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

// TODO: Overwrite with config file
#define PORT 5000
#define BUFFER_SIZE 1024

struct ConnectedClient {
    int socket_fd;
    std::string type;
    std::string topic;
};

static std::vector<ConnectedClient> connectedClientList;
static std::mutex clientListMutex;

// thread tracking so we can join on shutdown
static std::vector<std::thread> clientThreads;
static std::mutex threadListMutex;

// server fd and running flag (single global used by main + handler)
static int server_fd = -1;
static std::atomic<bool> running(true);

// Setup serializer
static IMmwMessageSerializer* g_serializer = nullptr;

// --- helper: safely route message to subscribers ---
void routeMessageToSubscribers(const std::string& topic, const MmwMessage& msg) {
    if (topic.empty()) {
        return;
    }

    // Copy subscriber sockets safely
    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto& client : connectedClientList) {
            if (client.type == "subscriber" && client.topic == topic) {
                targets.push_back(client.socket_fd);
            }
        }
    }

    // Serialize once
    std::string serialized = g_serializer->serialize(msg) + "\n";

    // Send to all targets
    for (int fd : targets) {
        ssize_t n = send(fd, serialized.c_str(), serialized.size(), MSG_NOSIGNAL);
        if (n < 0) {
            int err = errno;
            spdlog::error("send to subscriber fd={} failed {}", fd, err);
            std::lock_guard<std::mutex> lock(clientListMutex);
            connectedClientList.erase(
                std::remove_if(connectedClientList.begin(), connectedClientList.end(),
                    [fd](const ConnectedClient& c) { return c.socket_fd == fd; }),
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
            [client_fd](const ConnectedClient& c) {
                return c.socket_fd == client_fd;
                }
            ),
        connectedClientList.end()
    );
}

void handleClient(int client_fd) {
    char buffer[BUFFER_SIZE];
    std::string incomingBuffer;

    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) {
            if (n == 0) {
                // orderly shutdown from peer
            } else if (errno == EINTR) {
                continue;
            } else {
                spdlog::error("recv error on fd {}:{}", client_fd, strerror(errno));
            }
            break;
        }

        incomingBuffer.append(buffer, n);

        size_t pos;
        while ((pos = incomingBuffer.find('\n')) != std::string::npos) {
            std::string oneMsg = incomingBuffer.substr(0, pos);
            incomingBuffer.erase(0, pos + 1);

            try {
                MmwMessage msg = g_serializer->deserialize(oneMsg);

                if (msg.type == "register") {
                    ConnectedClient newClient{client_fd, msg.payload, msg.topic};
                    {
                        std::lock_guard<std::mutex> lock(clientListMutex);
                        connectedClientList.push_back(newClient);
                    }
                    spdlog::info("Registered {} for topic {} (fd={})", msg.payload, msg.topic, client_fd);
                } 
                else if (msg.type == "unregister") {
                    std::lock_guard<std::mutex> lock(clientListMutex);
                    connectedClientList.erase(
                        std::remove_if(
                            connectedClientList.begin(),
                            connectedClientList.end(),
                            [&](const ConnectedClient& c) {
                                return c.socket_fd == client_fd && c.topic == msg.topic;
                            }),
                        connectedClientList.end());
                    spdlog::info("Unregistered client fd={} topic={}", client_fd, msg.topic);
                } 
                else if (msg.type == "publish") {
                    routeMessageToSubscribers(msg.topic, msg);
                }

            } catch (const std::exception& e) {
                spdlog::error("Failed to deserialize message: {}", e.what());
            }
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
        close(server_fd); // cause accept() to fail and break loop
        server_fd = -1;
    }
}

int main() {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    // Serializer initialization
    static JsonSerializer serializer;
    g_serializer = &serializer;

    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    // create server socket (global)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    // reuse addr for quick restart
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind"); return 1;
    }

    if (listen(server_fd, 16) < 0) {
        perror("listen"); return 1;
    }

    spdlog::info("Broker listening on port ", PORT);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!running) {
                break; // shutdown requested
            }
            if (errno == EINTR) {
                continue; // interrupted by signal, retry
            }
            perror("accept");
            continue;
        }

        spdlog::info("Client connected from {}:{}(fd={})", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);

        // spawn thread and track it so we can join on shutdown
        {
            std::lock_guard<std::mutex> lt(threadListMutex);
            clientThreads.emplace_back(std::thread(handleClient, client_fd));
        }
    }

    // wait for client threads to finish
    {
        std::lock_guard<std::mutex> lt(threadListMutex);
        for (auto &t : clientThreads) {
            if (t.joinable()) t.join();
        }
        clientThreads.clear();
    }

    // final cleanup: close any remaining client sockets
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto &c : connectedClientList) {
            if (c.socket_fd != -1) close(c.socket_fd);
        }
        connectedClientList.clear();
    }

    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }

    spdlog::info("Broker exited cleanly");
    return 0;
}
