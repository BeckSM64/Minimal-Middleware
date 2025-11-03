#include "BrokerPersistence.h"
#include <spdlog/spdlog.h>

BrokerPersistence::BrokerPersistence(const std::string& dbPath)
    : db_(nullptr), dbPath_(dbPath), running_(true)
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (sqlite3_open_v2(dbPath_.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        spdlog::error("Failed to open SQLite DB {}: {}", dbPath_, sqlite3_errmsg(db_));
        db_ = nullptr;
        return;
    }
    prepareDatabase();

    // Start background worker
    worker_ = std::thread([this]() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });
            if (!running_) break;

            MmwMessage msg = std::move(queue_.front());
            queue_.pop();
            lock.unlock();

            persistBlocking(msg);
        }
    });
}

BrokerPersistence::~BrokerPersistence() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        running_ = false;
    }
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();

    std::lock_guard<std::mutex> lock(dbMutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool BrokerPersistence::prepareDatabase() {
    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS messages ("
        "messageId INTEGER PRIMARY KEY,"
        "topic TEXT NOT NULL,"
        "payload BLOB NOT NULL,"
        "reliability INTEGER NOT NULL"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        spdlog::error("Failed to create messages table: {}", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// Public async interface
bool BrokerPersistence::persistMessage(const MmwMessage& msg) {
    if (!db_ || !running_) return false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push(msg);
    }
    cv_.notify_one();
    return true;
}

// Actual SQLite write (blocking, used only by worker thread)
bool BrokerPersistence::persistBlocking(const MmwMessage& msg) {
    std::lock_guard<std::mutex> lock(dbMutex_);

    sqlite3_stmt* stmt = nullptr;
    const char* insertSQL = "INSERT INTO messages (messageId, topic, payload, reliability) VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db_, insertSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int(stmt, 1, msg.messageId);
    sqlite3_bind_text(stmt, 2, msg.topic.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, msg.payload.data(), static_cast<int>(msg.payload.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, msg.reliability ? 1 : 0);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to execute statement: {}", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

uint32_t BrokerPersistence::getNextMessageId() {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT MAX(messageId) FROM messages;";
    uint32_t nextId = 1;

    std::lock_guard<std::mutex> lock(dbMutex_);
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int maxId = sqlite3_column_int(stmt, 0);
            nextId = maxId + 1;
        }
    }
    sqlite3_finalize(stmt);
    return nextId;
}
