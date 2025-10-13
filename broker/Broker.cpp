#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

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

std::vector<ConnectedClient> connectedClientList;
std::mutex clientListMutex;

// Parses "TOPIC:topic\nMESSAGE:message" into a struct
ParsedMessage parseMessage(const char* received) {
    ParsedMessage result;

    std::string str(received);
    size_t newlinePos = str.find('\n');
    if (newlinePos == std::string::npos) {
        std::cerr << "Invalid message format\n";
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

void routeMessageToSubscribers(const std::string& topic, const std::string& message) {
    std::lock_guard<std::mutex> lock(clientListMutex); // protect the vector

    for (auto& client : connectedClientList) {
        if (client.type == "subscriber" && client.topic == topic) {
            int n = send(client.socket_fd, message.c_str(), message.size(), 0);
            if (n < 0) {
                perror("send to subscriber");
            }
        }
    }
}

void handleClient(int client_fd) {
    char buffer[BUFFER_SIZE];

    while (true) {

        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) break;

        // std::cout << "Received from client: " << buffer << std::endl;
        buffer[n] = '\0';

        // Determine if this is a new topic registration, or a normal message
        if (strncmp(buffer, "PUB_REGISTER:", 13) == 0 || strncmp(buffer, "SUB_REGISTER:", 13) == 0) {

            // Determine if publisher or subscriber registration
            std::string connectionTypeAsString;
            if (strncmp(buffer, "PUB", 3) == 0) {
                connectionTypeAsString = "publisher";
            } else {
                connectionTypeAsString = "subscriber";
            }

            // Update client list
            std::string topic = buffer + 13;

            // Add client to list
            ConnectedClient newClient;
            newClient.socket_fd = client_fd;
            newClient.type = connectionTypeAsString;
            newClient.topic = topic;

            {
                std::lock_guard<std::mutex> lock(clientListMutex);
                connectedClientList.push_back(newClient);
                std::cout << "Client list updated: " << std::endl;
                for (int i = 0; i < connectedClientList.size(); i++) {
                    std::cout << " - " << connectedClientList[i].topic << std::endl;
                    std::cout << " - " << connectedClientList[i].type << std::endl;
                }
            }
        } else {
            // Route message to appropriate subscribers
            std::cout << "This is a normal message: " << buffer << std::endl;
            ParsedMessage pm = parseMessage(buffer);
            routeMessageToSubscribers(pm.topic, pm.message);
        }
    }

    close(client_fd);
    std::cout << "Client disconnected" << std::endl;

    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        connectedClientList.erase(
            std::remove_if(connectedClientList.begin(), connectedClientList.end(),
                [client_fd](const ConnectedClient& c) {
                    return c.socket_fd == client_fd;
            }),
            connectedClientList.end()
        );
    }
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket"); return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind"); return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen"); return 1;
    }

    std::cout << "Broker listening on port " << PORT << std::endl;

    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        std::cout << "Client connected!" << std::endl;
        std::thread t(handleClient, client_fd);
        t.detach(); // Let the client run independently
    }

    close(server_fd);
    std::cout << "Broker exiting" << std::endl;
}
