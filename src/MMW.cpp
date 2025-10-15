#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <vector>
#include <iostream>

#include "MMW.h"
#include "ConfigFileParser.h"
#include "IMmwMessageSerializer.h"
#include "JsonSerializer.h"
#include <fcntl.h>

#define BUFFER_SIZE 1024

static std::string hostname = "127.0.0.1";
static int brokerPort = 5000;
static struct sockaddr_in server_addr;
static std::atomic<bool> running{false};
static std::map<std::string, int> publisherTopicToSocketFdMap;
static std::map<std::string, int> subscriberTopicToSocketFdMap;
static std::mutex socketListMutex;
static std::vector<std::thread> subscriberThreads;
static std::vector<std::atomic<bool>*> subscriberRunFlags;
static IMmwMessageSerializer* g_serializer = nullptr;

/**
 * Initialize library settings
 */
int mmw_initialize(const char* configPath) {
    ConfigFileParser configParser;
    configParser.parseConfigFile(configPath);
    hostname = configParser.getBrokerIp();
    brokerPort = configParser.getBrokerPort();

    // Initialize the serializer
    g_serializer = new JsonSerializer();

    return 1;
}

/**
 * Create a publisher
 */
int mmw_create_publisher(const char* topic) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(brokerPort);
    inet_pton(AF_INET, hostname.c_str(), &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return -1;
    }

    // JSON registration message for publisher
    MmwMessage msg;
    msg.type = "register";
    msg.topic = topic;
    msg.payload = "publisher";

    std::string registrationData = g_serializer->serialize(msg) + "\n";
    send(sock_fd, registrationData.c_str(), registrationData.size(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        std::lock_guard<std::mutex> lock(socketListMutex);
        publisherTopicToSocketFdMap[topic] = sock_fd;
    }

    std::cout << "Publisher connected to broker at " << hostname << ":" << brokerPort << std::endl;
    return 0;
}

/**
 * Create a subscriber
 */
int mmw_create_subscriber(const char* topic, void (*mmw_callback)(const char*)) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(brokerPort);
    inet_pton(AF_INET, hostname.c_str(), &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return -1;
    }

    // JSON registration message for subscriber
    MmwMessage msg;
    msg.type = "register";
    msg.topic = topic;
    msg.payload = "subscriber";

    std::string registrationData = g_serializer->serialize(msg) + "\n";
    send(sock_fd, registrationData.c_str(), registrationData.size(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto runningFlag = new std::atomic<bool>(true);
    std::thread t([sock_fd, runningFlag, mmw_callback]() {
        char buffer[BUFFER_SIZE];
        std::string incomingBuffer;

        while (*runningFlag) {
            memset(buffer, 0, sizeof(buffer));
            int n = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                incomingBuffer.append(buffer, n);
                size_t pos;
                while ((pos = incomingBuffer.find('\n')) != std::string::npos) {
                    std::string oneMsg = incomingBuffer.substr(0, pos);
                    incomingBuffer.erase(0, pos + 1);
                    try {
                        MmwMessage msg = g_serializer->deserialize(oneMsg);
                        if (msg.type == "publish")
                            mmw_callback(msg.payload.c_str());
                    } catch (const std::exception& e) {
                        std::cerr << "Subscriber failed to deserialize: " << e.what() << std::endl;
                    }
                }
            } else if (n == 0) {
                break; // broker closed connection
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        close(sock_fd);
        std::cout << "Subscriber listener thread exiting\n";
    });

    subscriberThreads.push_back(std::move(t));
    subscriberRunFlags.push_back(runningFlag);
    return 0;
}

/**
 * Publish a message
 */
int mmw_publish(const char* topic, const char* payload) {
    auto it = publisherTopicToSocketFdMap.find(topic);
    if (it == publisherTopicToSocketFdMap.end()) {
        std::cerr << "No publisher socket for topic: " << topic << std::endl;
        return -1;
    }

    int sock_fd = it->second;
    MmwMessage msg;
    msg.topic = topic;
    msg.payload = payload;
    msg.type = "publish";

    std::string serialized = g_serializer->serialize(msg) + "\n";
    send(sock_fd, serialized.c_str(), serialized.size(), 0);
    return 0;
}

/**
 * Clean up publishers/subscribers
 */
int mmw_cleanup() {

    // Cleanup publisher sockets
    for (auto& pair : publisherTopicToSocketFdMap) {
        int sock_fd = pair.second;
        if (sock_fd != -1) {
            MmwMessage msg;
            msg.type = "unregister";
            msg.topic = pair.first;
            msg.payload = "";

            std::string doneMsg = g_serializer->serialize(msg) + "\n";
            send(sock_fd, doneMsg.c_str(), doneMsg.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            close(sock_fd);
            std::cout << "Publisher socket closed for topic: " << pair.first << std::endl;
        }
    }
    publisherTopicToSocketFdMap.clear();

    // Stop subscriber threads
    {
        std::lock_guard<std::mutex> lock(socketListMutex);
        for (auto* flag : subscriberRunFlags) {
            *flag = false;
        }
    }

    // Wait for threads to finish
    for (auto& t : subscriberThreads) {
        if (t.joinable()) t.join();
    }
    subscriberThreads.clear();

    // Close subscriber sockets
    for (auto& pair : subscriberTopicToSocketFdMap) {
        int sock_fd = pair.second;
        if (sock_fd != -1) {
            MmwMessage msg;
            msg.type = "unregister";
            msg.topic = pair.first;
            msg.payload = "";

            std::string doneMsg = g_serializer->serialize(msg) + "\n";
            send(sock_fd, doneMsg.c_str(), doneMsg.size(), 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            close(sock_fd);
            std::cout << "Subscriber socket closed for topic: " << pair.first << std::endl;
        }
    }
    subscriberTopicToSocketFdMap.clear();

    // Cleanup running flags
    for (auto* flag : subscriberRunFlags) {
        delete flag;
    }
    subscriberRunFlags.clear();

    // Cleanup serializer
    if (g_serializer) {
        delete g_serializer;
        g_serializer = nullptr;
    }

    return 0;
}
