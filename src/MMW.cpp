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
#include "ITransport.h"
#include "TcpTransport.h"

static std::string hostname = "127.0.0.1";
static int brokerPort = 5000;
static struct sockaddr_in server_addr;
static std::atomic<bool> running{false};

static std::map<std::string, ITransport *> publisherTopicToTransportMap;
static std::map<std::string, ITransport *> subscriberTopicToTransportMap;
static std::mutex transportListMutex;

static std::map<std::string, int> publisherTopicToSocketFdMap;
static std::map<std::string, int> subscriberTopicToSocketFdMap;
static std::mutex socketListMutex;

static std::vector<std::thread> subscriberThreads;
static std::vector<std::atomic<bool>*> subscriberRunFlags;
static IMmwMessageSerializer* g_serializer = nullptr;

static std::map<int, std::mutex> socketSendMutexes;
static std::map<ITransport *, std::mutex> trasnportSendMutexes;
static std::mutex socketSendMutexMapLock;
static std::mutex trasnportSendMutexMapLock;

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

/**
 * Helper function to send a length-prefixed message
 */
inline MmwResult sendMessage(int sock_fd, const std::string& data) {
    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> lock(socketSendMutexMapLock);
        mtx = &socketSendMutexes[sock_fd];
    }

    std::lock_guard<std::mutex> lock(*mtx);

    uint32_t len = htonl(data.size());

    if (SocketAbstraction::Send(sock_fd, &len, sizeof(len), 0) != sizeof(len)) {
        return MMW_ERROR;
    }

    if (SocketAbstraction::Send(sock_fd, data.data(), data.size(), 0) != (ssize_t)data.size()) {
        return MMW_ERROR;
    }

    return MMW_OK;
}

inline MmwResult sendMessage(ITransport *transport, const std::string& data) {
    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> lock(trasnportSendMutexMapLock);
        mtx = &trasnportSendMutexes[transport];
    }

    std::lock_guard<std::mutex> lock(*mtx);

    // uint32_t len = htonl(data.size());

    // if (SocketAbstraction::Send(sock_fd, &len, sizeof(len), 0) != sizeof(len)) {
    //     return MMW_ERROR;
    // }

    // if (SocketAbstraction::Send(sock_fd, data.data(), data.size(), 0) != (ssize_t)data.size()) {
    //     return MMW_ERROR;
    // }

    transport->Send(data);

    return MMW_OK;
}

/**
 * Sets the log level for the library
 */
void mmw_set_log_level(MmwLogLevel level) {
    switch (level) {
        case MMW_LOG_LEVEL_ERROR:
            spdlog::set_level(spdlog::level::err);
            break;
        case MMW_LOG_LEVEL_WARN:
            spdlog::set_level(spdlog::level::warn);
            break;
        case MMW_LOG_LEVEL_INFO:
            spdlog::set_level(spdlog::level::info);
            break;
        case MMW_LOG_LEVEL_DEBUG:
            spdlog::set_level(spdlog::level::debug);
            break;
        case MMW_LOG_LEVEL_TRACE:
            spdlog::set_level(spdlog::level::trace);
            break;
        default:
            spdlog::set_level(spdlog::level::off);
            break;
    }
}

/**
 * Initialize library settings
 */
MmwResult mmw_initialize(const char* brokerIp, unsigned short port) {

    if (!brokerIp || port == 0) {
        spdlog::error("No broker IP or port provided");
        return MMW_ERROR;
    }

    hostname = brokerIp;
    brokerPort = port;

    g_serializer = CreateSerializer();
    if (!g_serializer) {
        spdlog::error("Failed to create serializer");
        return MMW_ERROR;
    }

    SocketAbstraction::SocketStartup();
    return MMW_OK;
}

/**
 * Create a publisher
 */
MmwResult mmw_create_publisher(const char* topic) {
    SocketAbstraction::SocketStartup();

    // Check that the serializer was set via mmw_initialize
    if (g_serializer == nullptr) {
        spdlog::error("Serializer not set. You may have forgotten to call mmw_initialize");
        return MMW_ERROR;
    }

    ITransport *transport = new TcpTransport();

    if (transport->Initialize() == MMW_ERROR) {
        return MMW_ERROR;
    }

    // Registration message
    MmwMessage msg{0, "register", topic, "publisher"};

    try {
        if (sendMessage(transport, g_serializer->serialize(msg)) == MMW_ERROR) {
            spdlog::error("Failed to send registration for publisher: {}", topic);
            // SocketAbstraction::SocketClose(sock_fd);
            return MMW_ERROR;
        }
    } catch (const std::exception& e) {
        spdlog::error("Publisher serialization failed for {}: {}", topic, e.what());
        // SocketAbstraction::SocketClose(sock_fd);
        return MMW_ERROR;
    }

    {
        std::lock_guard<std::mutex> transportLock(transportListMutex);
        publisherTopicToTransportMap[topic] = transport;
    }
    
    spdlog::info("Publisher connected to broker at {}:{}", hostname, brokerPort);
    return MMW_OK;
}

typedef std::function<void(const MmwMessage&)> SubscriberCallback;
void subscriberThreadFunc(
    ITransport* transport,
    std::atomic<bool>* runningFlag,
    SubscriberCallback callback)
{
    while (*runningFlag) {

        std::string data;

        if (transport->Recv(data) == MMW_ERROR) {
            spdlog::error("FAILING TO RECEIVE");
            break;
        }

        if (data.empty()) {
            continue;
        }

        try {
            MmwMessage msg =
                callback == nullptr
                    ? g_serializer->deserialize(data)
                    : g_serializer->deserialize_raw(data);

            if (msg.type == "publish") {

                if (msg.reliability) {

                    MmwMessage ackMsg;
                    ackMsg.messageId = msg.messageId;
                    ackMsg.type = "ack";
                    ackMsg.topic = msg.topic;

                    if (sendMessage(transport, g_serializer->serialize(ackMsg)) == MMW_ERROR) {
                        spdlog::error("Failed to send ACK for {}", ackMsg.messageId);
                    }
                }

                callback(msg);
            }

        } catch (const std::exception& e) {
            spdlog::error("Subscriber failed to deserialize: {}", e.what());
        }
    }

    spdlog::info("Subscriber listener thread exiting");
}

// Heartbeat thread
void heartbeatThreadFunc(ITransport* transport, std::atomic<bool>* runningFlag, int intervalMs) {
    auto lastHeartbeatTime = std::chrono::steady_clock::now();
    while (*runningFlag) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeatTime);
        if (elapsed.count() >= intervalMs) {
            MmwMessage hbMsg;
            hbMsg.type = "heartbeat";
            if(sendMessage(transport, g_serializer->serialize(hbMsg)) == MMW_ERROR) {
                spdlog::error("Failed to send hearbeat");
            }
            lastHeartbeatTime = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

MmwResult createSubscriberInternal(const char* topic, std::function<void(const MmwMessage&)> callback) {

    // Check that the serializer was set via mmw_initialize
    if (g_serializer == nullptr) {
        spdlog::error("Serializer not set. You may have forgotten to call mmw_initialize");
        return MMW_ERROR;
    }

    ITransport *transport = new TcpTransport();

    if (transport->Initialize() == MMW_ERROR) {
        return MMW_ERROR;
    }

    MmwMessage msg{0, "register", topic, "subscriber"};
    try {
        if (sendMessage(transport, g_serializer->serialize(msg)) == MMW_ERROR) {
            spdlog::error("Failed to send registration for subscriber: {}", topic);
            // SocketAbstraction::SocketClose(sock_fd);
            return MMW_ERROR;
        }
    } catch (const std::exception& e) {
        spdlog::error("Subscriber serialization failed for {}: {}", topic, e.what());
        // SocketAbstraction::SocketClose(sock_fd);
        return MMW_ERROR;
    }

    auto runningFlag = new std::atomic<bool>(true);

    {
        std::lock_guard<std::mutex> lock(socketListMutex);
        subscriberTopicToTransportMap[topic] = transport;
    }

    std::thread t(subscriberThreadFunc, transport, runningFlag, callback);
    subscriberThreads.push_back(std::move(t));

    std::thread hbThread(heartbeatThreadFunc, transport, runningFlag, 1000);
    subscriberThreads.push_back(std::move(hbThread));

    subscriberRunFlags.push_back(runningFlag);
    return MMW_OK;
}

/**
 * Create subscriber
 */
MmwResult mmw_create_subscriber(const char* topic, void (*cb)(const char*, const char*)) {
    return createSubscriberInternal(topic, [cb, topic](const MmwMessage& msg) {
        cb(topic, msg.payload.c_str());
    });
}

/**
 * Create subscriber for raw payload
 */
MmwResult mmw_create_subscriber_raw(const char* topic, void (*cb)(const char*, void*)) {
    return createSubscriberInternal(topic, [cb, topic](const MmwMessage& msg) {
        cb(topic, msg.payload_raw);
    });
}

MmwResult mmw_publish(const char* topic, const char* payload, MmwReliability reliability) {

    auto it = publisherTopicToTransportMap.find(topic);
    if (it == publisherTopicToTransportMap.end()) {
        spdlog::error("No existing publisher for topic: {}", topic);
        return MMW_ERROR;
    }

    ITransport *transport = it->second;
    MmwMessage msg{0, "publish", topic, payload};
    msg.reliability = reliability;

    try {
        if (sendMessage(transport, g_serializer->serialize(msg)) == MMW_ERROR) {
            spdlog::error("Failed to send message on topic {}", topic);
            return MMW_ERROR;
        }
    } catch (const std::exception& e) {
        spdlog::error("Publish serialization failed on topic {}: {}", topic, e.what());
        return MMW_ERROR;
    }

    return MMW_OK;
}

MmwResult mmw_publish_raw(const char* topic, void* payload, size_t size, MmwReliability reliability) {
    auto it = publisherTopicToTransportMap.find(topic);
    if (it == publisherTopicToTransportMap.end()) {
        spdlog::error("No existing publisher for topic: {}", topic);
        return MMW_ERROR;
    }

    ITransport* transport = it->second;
    MmwMessage msg{0, "publish", topic, "", payload, size};
    msg.reliability = reliability;

    try {
        if (sendMessage(transport, g_serializer->serialize_raw(msg)) == MMW_ERROR) {
            spdlog::error("Failed to send message on topic {}", topic);
            return MMW_ERROR;
        }
    } catch (const std::exception& e) {
        spdlog::error("Raw publish serialization failed on topic {}: {}", topic, e.what());
        return MMW_ERROR;
    }

    return MMW_OK;
}

/**
 * Delete publisher
 */
MmwResult mmw_delete_publisher(const char* topic) {
    auto it = publisherTopicToTransportMap.find(topic);
    if (it == publisherTopicToTransportMap.end()) {
        return MMW_ERROR;
    }

    ITransport *transport = it->second;

    MmwMessage msg{0, "unregister", topic, ""};
    if (sendMessage(transport, g_serializer->serialize(msg)) == MMW_ERROR) {
        spdlog::error("Failed to unregister publisher for topic {}", topic);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // SocketAbstraction::SocketClose(sock_fd);
    delete transport;
    transport = nullptr;

    publisherTopicToTransportMap.erase(it);

    spdlog::info("Publisher socket closed for topic: {}", topic);
    return MMW_OK;
}

/**
 * Delete subscriber
 */
MmwResult mmw_delete_subscriber(const char* topic) {
    auto it = subscriberTopicToSocketFdMap.find(topic);
    if (it == subscriberTopicToSocketFdMap.end()) {
        return MMW_ERROR;
    }

    int sock_fd = it->second;

    // Ask broker to unregister (best-effort)
    MmwMessage msg{0, "unregister", topic, ""};
    if (sendMessage(sock_fd, g_serializer->serialize(msg)) == MMW_ERROR) {
        spdlog::error("Failed to unregister subscriber for topic {}", topic);
    }

    // Give broker a moment, then force unblock recv()
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    SocketAbstraction::SocketClose(sock_fd);

    subscriberTopicToSocketFdMap.erase(it);

    spdlog::info("Subscriber socket closed for topic: {}", topic);
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
            if (sendMessage(sock_fd, g_serializer->serialize(msg)) == MMW_ERROR) {
                spdlog::error("Failed to unregister publisher for topic {}", pair.first);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            SocketAbstraction::SocketClose(sock_fd);
            spdlog::info("Publisher socket closed for topic: {}", pair.first);
        }
    }
    publisherTopicToSocketFdMap.clear();

    // Close subscriber sockets
    for (auto& pair : subscriberTopicToSocketFdMap) {
        int sock_fd = pair.second;
        if (sock_fd != -1) {
            MmwMessage msg{0, "unregister", pair.first, ""};
            if (sendMessage(sock_fd, g_serializer->serialize(msg))) {
                spdlog::error("Failed to unregister subscriber for topic {}", pair.first);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            SocketAbstraction::SocketClose(sock_fd);
            spdlog::info("Subscriber socket closed for topic: {}", pair.first);
        }
    }
    subscriberTopicToSocketFdMap.clear();

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
