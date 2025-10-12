#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include "MMW.h"

#define PORT 5000
#define BUFFER_SIZE 1024

static int sock_fd = -1;
static struct sockaddr_in server_addr;

/**
 * Create a publisher
 */

int mmw_create_publisher() {

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr); // TODO: Replace localhost IP with real IP

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        sock_fd = -1;
        return -1;
    }

    std::cout << "Connected to broker at " << "127.0.0.1" << ":" << PORT << std::endl;
    return 0;
}

/**
 * Create a subscriber
 */
int mmw_create_subscriber() {
    std::cout << "Create subscriber" << std::endl;
    return 0;
}

/**
 * Publish a message
 */
int mmw_publish(const char *message) {
    if (sock_fd == -1) {
        std::cerr << "Publisher not connected.\n";
        return -1;
    }
    send(sock_fd, message, strlen(message), 0);
    return 0;
}

/**
 * Clean up publishers/subscribers
 */
int mmw_cleanup() {

    // TODO: Update to a list/map of publishers and iterate through
    if (sock_fd != -1) {
        close(sock_fd);
        sock_fd = -1;
        std::cout << "Publisher socket closed.\n";
    }

    return 0;
}
