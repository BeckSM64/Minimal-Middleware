#pragma once
#include <string>
#include <mutex>
#include "MmwMessage.h"
#include <sqlite3.h>

class BrokerPersistence {
public:
    BrokerPersistence(const std::string& dbPath);
    ~BrokerPersistence();

    // Persist a message. Returns true on success.
    bool persistMessage(const MmwMessage& msg);
    uint32_t getNextMessageId();

private:
    bool prepareDatabase();

    sqlite3* db_;
    std::mutex dbMutex_;
    std::string dbPath_;
};
