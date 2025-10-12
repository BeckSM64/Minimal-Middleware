#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#define PORT 5000
#define BUFFER_SIZE 1024

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) { perror("socket"); return 1; }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to broker!" << std::endl;

    while (true) {
        std::string msg;
        std::cout << "Enter message: ";
        std::getline(std::cin, msg);

        if (msg == "quit") {
            break;
        }

        send(sock, msg.c_str(), msg.size(), 0);

        memset(buffer, 0, BUFFER_SIZE);
        int n = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (n <= 0) break;

        std::cout << "Received echo: " << buffer << std::endl;
    }

    close(sock);
    std::cout << "Client exiting" << std::endl;
}
