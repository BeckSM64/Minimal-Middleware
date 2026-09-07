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
#include "BeastTransport.h"

struct Subscriber {
    ITransport* transport;
    std::thread listenerThread;
    std::thread heartbeatThread;
    std::atomic<bool>* running;
};

static std::string hostname = "127.0.0.1";
static int brokerPort = 5000;
static struct sockaddr_in server_addr;
static std::atomic<bool> running{false};

static std::map<std::string, ITransport *> publisherTopicToTransportMap;
static std::map<std::string, ITransport *> subscriberTopicToTransportMap;
static std::mutex transportListMutex;

static IMmwMessageSerializer* g_serializer = nullptr;

static std::map<ITransport *, std::mutex> trasnportSendMutexes;
static std::mutex trasnportSendMutexMapLock;

static std::vector<Subscriber> subscribers;

#ifdef _WIN32
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

inline MmwResult sendMessage(ITransport *transport, const std::string& data) {
    std::mutex* mtx;
    {
        std::lock_guard<std::mutex> lock(trasnportSendMutexMapLock);
        mtx = &trasnportSendMutexes[transport];
    }

    std::lock_guard<std::mutex> lock(*mtx);

    return transport->Send(data);
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

    // ITransport *transport = new TcpTransport();
    ITransport *transport = new BeastTransport();

    if (transport->Initialize() == MMW_ERROR) {
        return MMW_ERROR;
    }

    // Registration message
    MmwMessage msg{0, "register", topic, "publisher"};

    try {
        if (sendMessage(transport, g_serializer->serialize(msg)) == MMW_ERROR) {
            spdlog::error("Failed to send registration for publisher: {}", topic);
            transport->Close();
            return MMW_ERROR;
        }
    } catch (const std::exception& e) {
        spdlog::error("Publisher serialization failed for {}: {}", topic, e.what());
        transport->Close();
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
void subscriberThreadFunc(ITransport* transport, std::atomic<bool>* runningFlag, SubscriberCallback callback) {
    while (*runningFlag) {

        std::string data;

        if (transport->Recv(data) == MMW_ERROR) {
            spdlog::error("Failed to recv incoming messages...");
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

    // ITransport *transport = new TcpTransport();
    ITransport *transport = new BeastTransport();

    if (transport->Initialize() == MMW_ERROR) {
        return MMW_ERROR;
    }

    MmwMessage msg{0, "register", topic, "subscriber"};
    try {
        if (sendMessage(transport, g_serializer->serialize(msg)) == MMW_ERROR) {
            spdlog::error("Failed to send registration for subscriber: {}", topic);
            transport->Close();
            return MMW_ERROR;
        }
    } catch (const std::exception& e) {
        spdlog::error("Subscriber serialization failed for {}: {}", topic, e.what());
        transport->Close();
        return MMW_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(transportListMutex);
        subscriberTopicToTransportMap[topic] = transport;
    }

    Subscriber subscriber;
    subscriber.transport = transport;
    subscriber.running = new std::atomic<bool>(true);

    subscriber.listenerThread =
        std::thread(subscriberThreadFunc, transport, subscriber.running, callback);

    subscriber.heartbeatThread =
        std::thread(heartbeatThreadFunc, transport, subscriber.running, 1000);

    subscribers.push_back(std::move(subscriber));
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
    transport->Close();
    delete transport;
    transport = nullptr;

    publisherTopicToTransportMap.erase(it);

    spdlog::info("Publisher deleted for topic: {}", topic);
    return MMW_OK;
}

/**
 * Delete subscriber
 */
MmwResult mmw_delete_subscriber(const char* topic) {
    auto it = subscriberTopicToTransportMap.find(topic);
    if (it == subscriberTopicToTransportMap.end()) {
        return MMW_ERROR;
    }

    ITransport* transport = it->second;

    // Ask broker to unregister (best-effort)
    MmwMessage msg{0, "unregister", topic, ""};
    if (sendMessage(transport, g_serializer->serialize(msg)) == MMW_ERROR) {
        spdlog::error("Failed to unregister subscriber for topic {}", topic);
    }

    auto subscriberIt = std::find_if(
        subscribers.begin(),
        subscribers.end(),
        [transport](const Subscriber& subscriber) {
            return subscriber.transport == transport;
        }
    );

    if (subscriberIt != subscribers.end()) {
        // Stop subscriber and heartbeat threads
        subscriberIt->running->store(false);

        // Unblock listenerThread's Recv()
        transport->Close();

        // Wait for both threads to finish before destroying anything
        if (subscriberIt->listenerThread.joinable()) {
            subscriberIt->listenerThread.join();
        }

        if (subscriberIt->heartbeatThread.joinable()) {
            subscriberIt->heartbeatThread.join();
        }

        delete subscriberIt->running;
        delete subscriberIt->transport;

        subscribers.erase(subscriberIt);
    }

    subscriberTopicToTransportMap.erase(it);
    spdlog::info("Subscriber deleted for topic: {}", topic);

    return MMW_OK;
}


/**
 * Clean up publishers/subscribers
 */
MmwResult mmw_cleanup() {
    // Stop and clean up all subscribers.
    for (auto& subscriber : subscribers) {
        if (subscriber.transport != nullptr) {
            MmwMessage msg{0, "unregister", "", ""};

            // Find the topic associated with this transport.
            for (const auto& pair : subscriberTopicToTransportMap) {
                if (pair.second == subscriber.transport) {
                    msg.topic = pair.first;
                    break;
                }
            }

            if (g_serializer != nullptr) {
                if (sendMessage(
                        subscriber.transport,
                        g_serializer->serialize(msg)) == MMW_ERROR) {
                    spdlog::error(
                        "Failed to unregister subscriber for topic {}",
                        msg.topic);
                }
            }

            // Stop Recv() and heartbeat thread.
            subscriber.running->store(false);
            subscriber.transport->Close();
        }
    }

    // The transport close above wakes Recv(), so now join both threads.
    for (auto& subscriber : subscribers) {
        if (subscriber.listenerThread.joinable()) {
            subscriber.listenerThread.join();
        }

        if (subscriber.heartbeatThread.joinable()) {
            subscriber.heartbeatThread.join();
        }

        delete subscriber.running;
        subscriber.running = nullptr;

        delete subscriber.transport;
        subscriber.transport = nullptr;
    }

    subscribers.clear();
    subscriberTopicToTransportMap.clear();


    // Clean up all publishers.
    for (auto& pair : publisherTopicToTransportMap) {
        ITransport* transport = pair.second;

        if (transport != nullptr) {
            if (g_serializer != nullptr) {
                MmwMessage msg{0, "unregister", pair.first, ""};

                if (sendMessage(
                        transport,
                        g_serializer->serialize(msg)) == MMW_ERROR) {
                    spdlog::error(
                        "Failed to unregister publisher for topic {}",
                        pair.first);
                }
            }

            transport->Close();
            delete transport;
        }
    }

    publisherTopicToTransportMap.clear();


    // Destroy serializer last, after no threads can use it.
    if (g_serializer != nullptr) {
        delete g_serializer;
        g_serializer = nullptr;
    }

    return MMW_OK;
}

