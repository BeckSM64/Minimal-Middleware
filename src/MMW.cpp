#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <iostream>

#include "MMW.h"

#define PORT 5000
#define BUFFER_SIZE 1024

static struct sockaddr_in server_addr;
static std::atomic<bool> running{false};
static std::map<std::string, int> publisherTopicToSocketFdMap;
static std::map<std::string, int> subscriberTopicToSocketFdMap;
static std::mutex socketListMutex;

/**
 * Create a publisher
 */

int mmw_create_publisher(const char* topic) {

    int sock_fd = -1;
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

    // Add socket_fd to global list
    {
        std::lock_guard<std::mutex> lock(socketListMutex);
        publisherTopicToSocketFdMap[topic] = sock_fd;
    }

    std::cout << "Publisher connected to broker at " << "127.0.0.1" << ":" << PORT << std::endl;
    return 0;
}

/**
 * Create a subscriber
 */
int mmw_create_subscriber(const char* topic, void (*mmw_callback)(const char*)) {

    // Handle connection to broker
    int sock_fd = -1;
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

    // Add socket_fd to global list
    {
        std::lock_guard<std::mutex> lock(socketListMutex);
        subscriberTopicToSocketFdMap[topic] = sock_fd;
    }

    std::cout << "Subscriber connected to broker at " << "127.0.0.1" << ":" << PORT << std::endl;

    // Stay alive, wait for message to trigger callback
    static std::atomic<bool> running{true};

    // TODO: Add threads to a list so they can be joined when cleanup is called
    std::thread([sock_fd, mmw_callback]() {
        char buffer[1024];
        while (running) {
            memset(buffer, 0, sizeof(buffer));
            int n = recv(sock_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT); // non-blocking

            // TODO: Do proper cleanup if this errors instead of just breaking
            if (n > 0) {
                mmw_callback(buffer);
            } else if (n == 0) {
                break;
            } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "Subscriber listener thread exiting\n";
    }).detach();

    return 0;
}

/**
 * Publish a message
 */
int mmw_publish(const char* topic, const char *message) {

    auto it = publisherTopicToSocketFdMap.find(topic);
    if (it == publisherTopicToSocketFdMap.end()) {
        std::cerr << "Publisher not connected for topic: " << topic << std::endl;
        return -1;
    }
    int sock_fd = it->second;

    if (sock_fd == -1) {
        std::cerr << "Publisher not connected.\n";
        return -1;
    }

    // TODO: Should publishes be non blocking?
    send(sock_fd, message, strlen(message), 0);
    return 0;
}

/**
 * Clean up publishers/subscribers
 */
int mmw_cleanup() {

    // Cleanup publishers
    for (auto pair : publisherTopicToSocketFdMap) {
        int sock_fd = pair.second;
        if (sock_fd != -1) {
            close(sock_fd);
            sock_fd = -1;
            std::cout << "Publisher socket closed for topic: " << pair.first << std::endl;
        }
    }

    // Cleanup subscribers
    for (auto pair : subscriberTopicToSocketFdMap) {
        int sock_fd = pair.second;
        if (sock_fd != -1) {
            close(sock_fd);
            sock_fd = -1;
            std::cout << "Subscriber socket closed for topic: " << pair.first << std::endl;
        }
    }

    // Clear the maps
    publisherTopicToSocketFdMap.clear();
    subscriberTopicToSocketFdMap.clear();

    return 0;
}
