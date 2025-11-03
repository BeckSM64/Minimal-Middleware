#include <unistd.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <fcntl.h>

#include "MMW.h"
#include "IMmwMessageSerializer.h"
#include "SerializerAbstraction.h"
#include "SocketAbstraction.h"

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
 * Helper function to send a length-prefixed message
 */
inline bool sendMessage(int sock_fd, const std::string& data) {

    SocketAbstraction::SocketStartup();

    uint32_t len = htonl(data.size());
    if (SocketAbstraction::Send(sock_fd, &len, sizeof(len), 0) != sizeof(len)) {
        return false;
    }
    if (SocketAbstraction::Send(sock_fd, (uint32_t*) data.data(), data.size(), 0) != (ssize_t)data.size()) {
        return false;
    }
    return true;
}

/**
 * Helper function to send subscriber heartbeat
 */
static void sendHeartbeat(int sock_fd, std::chrono::steady_clock::time_point& lastHeartbeatTime, int intervalMs) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeatTime);
    if (elapsed.count() >= intervalMs) {
        MmwMessage hbMsg;
        hbMsg.type = "heartbeat";
        sendMessage(sock_fd, g_serializer->serialize(hbMsg));
        lastHeartbeatTime = now;
    }
}

/**
 * Initialize library settings
 */
MmwResult mmw_initialize(const char* brokerIp, unsigned short port) {

    if (!brokerIp || port == 0) {
        return MMW_ERROR;
    }

    hostname = brokerIp;
    brokerPort = port;

    g_serializer = CreateSerializer();
    if (!g_serializer) {
        return MMW_ERROR;
    }

    return MMW_OK;
}

/**
 * Create a publisher
 */
MmwResult mmw_create_publisher(const char* topic) {

    SocketAbstraction::SocketStartup();

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return MMW_ERROR;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(brokerPort);
    SocketAbstraction::InetPtonAbstraction(AF_INET, hostname.c_str(), &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return MMW_ERROR;
    }

    // Registration message
    MmwMessage msg{0, "register", topic, "publisher"};

    if (!sendMessage(sock_fd, g_serializer->serialize(msg))) {
        spdlog::error("Failed to send registration for publisher: {}", topic);
        close(sock_fd);
        return MMW_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(socketListMutex);
        publisherTopicToSocketFdMap[topic] = sock_fd;
    }

    spdlog::info("Publisher connected to broker at {}:{}", hostname, brokerPort);
    return MMW_OK;
}

typedef std::function<void(const MmwMessage&)> SubscriberCallback;
void subscriberThreadFunc(int sock_fd, std::atomic<bool>* runningFlag, SubscriberCallback callback) {
    constexpr int HEARTBEAT_INTERVAL_MS = 1000;
    auto lastHeartbeatTime = std::chrono::steady_clock::now();

    while (*runningFlag) {
        uint32_t netLen;
        int n = SocketAbstraction::Recv(sock_fd, &netLen, sizeof(netLen), MSG_DONTWAIT);
        if (n > 0) {
            uint32_t msgLen = ntohl(netLen);
            if (msgLen > 0) {
                std::vector<char> buf(msgLen);
                n = SocketAbstraction::Recv(sock_fd, (uint32_t*) buf.data(), msgLen, MSG_WAITALL);
                if (n > 0) {
                    try {
                        MmwMessage msg = callback == nullptr
                            ? g_serializer->deserialize(std::string(buf.data(), msgLen))
                            : g_serializer->deserialize_raw(std::string(buf.data(), msgLen));

                        if (msg.type == "publish") {
                            callback(msg);

                            if (msg.reliability) {
                                MmwMessage ackMsg;
                                ackMsg.messageId = msg.messageId;
                                ackMsg.type = "ack";
                                ackMsg.topic = msg.topic;
                                sendMessage(sock_fd, g_serializer->serialize(ackMsg));
                                spdlog::info("ACK sent for {}", ackMsg.messageId);
                            }
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("Subscriber failed to deserialize: {}", e.what());
                    }
                }
            }
        }

        sendHeartbeat(sock_fd, lastHeartbeatTime, HEARTBEAT_INTERVAL_MS);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(sock_fd);
    spdlog::info("Subscriber listener thread exiting");
}

MmwResult createSubscriberInternal(
    const char* topic,
    std::function<void(const MmwMessage&)> callback
) {
    SocketAbstraction::SocketStartup();

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) { perror("socket"); return MMW_ERROR; }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(brokerPort);
    SocketAbstraction::InetPtonAbstraction(AF_INET, hostname.c_str(), &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return MMW_ERROR;
    }

    MmwMessage msg{0, "register", topic, "subscriber"};
    if (!sendMessage(sock_fd, g_serializer->serialize(msg))) {
        spdlog::error("Failed to send registration for subscriber: {}", topic);
        close(sock_fd);
        return MMW_ERROR;
    }

    auto runningFlag = new std::atomic<bool>(true);
    std::thread t(subscriberThreadFunc, sock_fd, runningFlag, callback);

    subscriberThreads.push_back(std::move(t));
    subscriberRunFlags.push_back(runningFlag);
    return MMW_OK;
}

/**
 * Create subscriber
 */
MmwResult mmw_create_subscriber(const char* topic, void (*cb)(const char*)) {
    return createSubscriberInternal(topic, [cb](const MmwMessage& msg) { cb(msg.payload.c_str()); });
}

/**
 * Create subscriber for raw payload
 */
MmwResult mmw_create_subscriber_raw(const char* topic, void (*cb)(void*)) {
    return createSubscriberInternal(topic, [cb](const MmwMessage& msg) { cb(msg.payload_raw); });
}

/**
 * Publish a message
 */
MmwResult mmw_publish(const char* topic, const char* payload, MmwReliability reliability) {
    auto it = publisherTopicToSocketFdMap.find(topic);
    if (it == publisherTopicToSocketFdMap.end()) {
        return MMW_ERROR;
    }

    int sock_fd = it->second;
    MmwMessage msg{0, "publish", topic, payload};
    msg.reliability = reliability;
    if (!sendMessage(sock_fd, g_serializer->serialize(msg))) {
        spdlog::error("Failed to send message on topic {}", topic);
        return MMW_ERROR;
    }
    return MMW_OK;
}

/**
 * Publish a message of raw bytes
 */
MmwResult mmw_publish_raw(const char* topic, void* payload, size_t size, MmwReliability reliability) {
    auto it = publisherTopicToSocketFdMap.find(topic);
    if (it == publisherTopicToSocketFdMap.end()) {
        return MMW_ERROR;
    }

    int sock_fd = it->second;
    MmwMessage msg{0, "publish", topic, "", payload, size};
    msg.reliability = reliability;
    if (!sendMessage(sock_fd, g_serializer->serialize_raw(msg))) {
        spdlog::error("Failed to send message on topic {}", topic);
        return MMW_ERROR;
    }
    return MMW_OK;
}

/**
 * Clean up publishers/subscribers
 */
MmwResult mmw_cleanup() {
    // Cleanup publisher sockets
    for (auto& pair : publisherTopicToSocketFdMap) {
        int sock_fd = pair.second;
        if (sock_fd != -1) {
            MmwMessage msg{0, "unregister", pair.first, ""};
            sendMessage(sock_fd, g_serializer->serialize(msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            close(sock_fd);
            spdlog::info("Publisher socket closed for topic: {}", pair.first);
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

    for (auto& t : subscriberThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    subscriberThreads.clear();

    // Close subscriber sockets
    for (auto& pair : subscriberTopicToSocketFdMap) {
        int sock_fd = pair.second;
        if (sock_fd != -1) {
            MmwMessage msg{0, "unregister", pair.first, ""};
            sendMessage(sock_fd, g_serializer->serialize(msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            close(sock_fd);
            spdlog::info("Subscriber socket closed for topic: {}", pair.first);
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

    SocketAbstraction::SocketCleanup();

    return MMW_OK;
}
