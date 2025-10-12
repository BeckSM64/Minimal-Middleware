#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

#define PORT 5000
#define BUFFER_SIZE 1024

struct ConnectedClient {
    int socket_fd;
    std::string type;
    std::string topic;
};

std::vector<ConnectedClient> connectedClientList;
std::mutex clientListMutex;

void handleClient(int client_fd) {
    char buffer[BUFFER_SIZE];

    // Add client to list (example data)
    ConnectedClient newClient;
    newClient.socket_fd = client_fd;
    newClient.type = "publisher";
    newClient.topic = "test topic";

    {
        std::lock_guard<std::mutex> lock(clientListMutex);
        connectedClientList.push_back(newClient);
        std::cout << "Client list updated: " << std::endl;
        for (int i = 0; i < connectedClientList.size(); i++) {
            std::cout << " - " << connectedClientList[i].topic << std::endl;
        }
    }

    // Test sending to subscriber
    // send(client_fd, "Subscriber should get this", strlen("Subscriber should get this"), 0);

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) break;

        std::cout << "Received from client: " << buffer << std::endl;

        // Echo back
        send(client_fd, buffer, n, 0);
    }

    close(client_fd);
    std::cout << "Client disconnected" << std::endl;
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
