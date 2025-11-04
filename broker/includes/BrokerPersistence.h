#pragma once
#include <string>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include "MmwMessage.h"
#include <sqlite3.h>

class BrokerPersistence {
public:
    BrokerPersistence(const std::string& dbPath);
    ~BrokerPersistence();

    // Queue message for async persistence
    bool persistMessage(const MmwMessage& msg);

    // Get the next messageId (based on DB max)
    uint32_t getNextMessageId();

private:
    // Blocking persistence function, used by worker thread
    bool persistBlocking(const MmwMessage& msg);

    bool prepareDatabase();

    sqlite3* db_;
    std::mutex dbMutex_;
    std::string dbPath_;

    // Async queue
    std::queue<MmwMessage> queue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool running_;
};
