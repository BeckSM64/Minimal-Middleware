#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>

#define PORT 5000
#define BUFFER_SIZE 1024

struct ConnectedClient {
    std::string ip_address;
    std::string type;
    std::string topic;
};

std::vector<ConnectedClient> connectedClientList;

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket"); return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Broker listening on port " << PORT << std::endl;

    client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);

    // Check if client connects
    if (client_fd < 0) {
        perror("accept"); return 1;
    }

    // Add client to connected client list
    ConnectedClient newClient;
    newClient.ip_address = "127.0.0.1";
    newClient.type = "publisher";
    newClient.topic = "test topic";

    connectedClientList.push_back(newClient);
    std::cout << "Client connected!" << std::endl;
    for (int i = 0; i < connectedClientList.size(); i++) {
        std::cout << connectedClientList.at(i).topic << std::endl;
    }

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) break;

        std::cout << "Received from client: " << buffer << std::endl;

        // Echo back
        send(client_fd, buffer, n, 0);
    }

    close(client_fd);
    close(server_fd);
    std::cout << "Broker exiting" << std::endl;
}
