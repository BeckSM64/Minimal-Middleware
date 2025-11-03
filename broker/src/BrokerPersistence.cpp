#include "BrokerPersistence.h"
#include <spdlog/spdlog.h>

BrokerPersistence::BrokerPersistence(const std::string& dbPath)
    : db_(nullptr), dbPath_(dbPath) 
{
    std::lock_guard<std::mutex> lock(dbMutex_);
    if (sqlite3_open_v2(dbPath_.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        spdlog::error("Failed to open SQLite DB {}: {}", dbPath_, sqlite3_errmsg(db_));
        db_ = nullptr;
        return;
    }
    prepareDatabase();
}

BrokerPersistence::~BrokerPersistence() {
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

bool BrokerPersistence::persistMessage(const MmwMessage& msg) {
    if (!db_) return false;

    std::lock_guard<std::mutex> lock(dbMutex_);

    sqlite3_stmt* stmt = nullptr;
    const char* insertSQL = "INSERT INTO messages (messageId, topic, payload, reliability) VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db_, insertSQL, -1, &stmt, nullptr) != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    // Bind values
    sqlite3_bind_int(stmt, 1, msg.messageId);
    sqlite3_bind_text(stmt, 2, msg.topic.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, msg.payload.data(), static_cast<int>(msg.payload.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, msg.reliability ? 1 : 0);

    // Execute
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to execute statement: {}", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}
