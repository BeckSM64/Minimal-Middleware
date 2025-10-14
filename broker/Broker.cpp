#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <signal.h>
#include <sys/socket.h> // MSG_NOSIGNAL
#include <errno.h>

#define PORT 5000
#define BUFFER_SIZE 1024

struct ConnectedClient {
    int socket_fd;
    std::string type;
    std::string topic;
};

struct ParsedMessage {
    std::string topic;
    std::string message;
};

static std::vector<ConnectedClient> connectedClientList;
static std::mutex clientListMutex;

// thread tracking so we can join on shutdown
static std::vector<std::thread> clientThreads;
static std::mutex threadListMutex;

// server fd and running flag (single global used by main + handler)
static int server_fd = -1;
static std::atomic<bool> running(true);

// --- helper: parse message ---
ParsedMessage parseMessage(const char* received) {
    ParsedMessage result;
    if (!received) return result;
    std::string str(received);
    size_t newlinePos = str.find('\n');
    if (newlinePos == std::string::npos) {
        // invalid format -> return empty topic/message
        return result;
    }
    std::string topicPart = str.substr(0, newlinePos);
    std::string messagePart = str.substr(newlinePos + 1);

    if (topicPart.find("TOPIC:") == 0)
        result.topic = topicPart.substr(6);
    if (messagePart.find("MESSAGE:") == 0)
        result.message = messagePart.substr(8);

    return result;
}

// --- helper: safely route message to subscribers ---
void routeMessageToSubscribers(const std::string& topic, const std::string& message) {

    // Check to make sure there's a topic before attempting to route
    if (topic.empty()) {
        return;
    }

    // copy targets under lock, then send outside lock to avoid blocking other ops
    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        for (auto& client : connectedClientList) {
            if (client.type == "subscriber" && client.topic == topic) {
                targets.push_back(client.socket_fd);
            }
        }
    }

    for (int fd : targets) {

        // MSG_NOSIGNAL works on Linux but won't on other platforms like Linux
        // TODO: Note this line as it may need to be updated in the future for other platforms
        ssize_t n = send(fd, message.c_str(), message.size(), MSG_NOSIGNAL);
        if (n < 0) {
            // Likely client disconnected; remove from list
            int err = errno;
            std::cerr << "send to subscriber fd=" << fd << " failed: " << strerror(err) << std::endl;
            std::lock_guard<std::mutex> lock(clientListMutex);
            connectedClientList.erase(
                std::remove_if(connectedClientList.begin(), connectedClientList.end(),
                    [fd](const ConnectedClient& c){
                        return c.socket_fd == fd;
                    }
                ),
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

    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) {
            if (n == 0) {
                // orderly shutdown from peer
                // std::cout << "recv returned 0 for fd " << client_fd << std::endl;
            } else {
                int err = errno;
                if (err == EINTR) {
                    continue;
                }
                // error; log then break
                std::cerr << "recv error on fd " << client_fd << ": " << strerror(err) << std::endl;
            }
            break;
        }

        buffer[n] = '\0';

        // Publisher/Subscriber registration
        if (strncmp(buffer, "PUB_REGISTER:", 13) == 0 || strncmp(buffer, "SUB_REGISTER:", 13) == 0) {

            std::string connectionTypeAsString = (strncmp(buffer, "PUB", 3) == 0) ? "publisher" : "subscriber";
            std::string topic = buffer + 13;

            ConnectedClient newClient;
            newClient.socket_fd = client_fd;
            newClient.type = connectionTypeAsString;
            newClient.topic = topic;

            {
                std::lock_guard<std::mutex> lock(clientListMutex);
                // avoid duplicates for same fd+topic+type
                bool exists = false;
                for (auto &c : connectedClientList) {
                    if (c.socket_fd == client_fd && c.topic == topic && c.type == connectionTypeAsString) {
                        exists = true; break;
                    }
                }
                if (!exists) {
                    connectedClientList.push_back(newClient);
                }
                std::cout << "Client list updated: " << std::endl;
                for (int i = 0; i < (int)connectedClientList.size(); ++i) {
                    std::cout << " - " << connectedClientList[i].topic << " (" << connectedClientList[i].type << ")" << std::endl;
                }
            }

            continue;
        }

        // Unregister publishers/subscribers that have cleaned up
        if (strncmp(buffer, "UNREGISTER:", 11) == 0) {
            std::string topic = buffer + 11;
            {
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.erase(
                    std::remove_if(connectedClientList.begin(), connectedClientList.end(),
                        [&](const ConnectedClient& c) {
                            return c.topic == topic && c.socket_fd == client_fd;
                        }
                    ),
                    connectedClientList.end()
                );
            }
            std::cout << "Client fd=" << client_fd << " unregistered topic=" << topic << std::endl;
            continue;
        }

        // Normal message, parse and route
        ParsedMessage pm = parseMessage(buffer);
        if (pm.topic.empty()) {
            std::cerr << "Received malformed message from fd=" << client_fd << ": " << buffer << std::endl;
        } else {
            routeMessageToSubscribers(pm.topic, pm.message);
        }
    }

    // cleanup for this client
    close(client_fd);
    removeClientByFd(client_fd);
    std::cout << "Client disconnected (fd=" << client_fd << ")\n";
}

void handleSignal(int signum) {
    std::cout << "\nSignal received (" << signum << "), shutting down broker..." << std::endl;
    running = false;
    if (server_fd != -1) {
        close(server_fd); // cause accept() to fail and break loop
        server_fd = -1;
    }
}

int main() {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

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

    std::cout << "Broker listening on port " << PORT << std::endl;

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

        std::cout << "Client connected from "
                  << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port)
                  << " (fd=" << client_fd << ")\n";

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

    std::cout << "Broker exited cleanly\n";
    return 0;
}
