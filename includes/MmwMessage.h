#pragma once
#include <string>

struct MmwMessage {
    uint32_t messageId;
    std::string type;    // "PUB_REGISTER", "SUB_REGISTER", "DATA", "UNREGISTER"
    std::string topic;   // topic name
    std::string payload; // message content, optional for register/unregister
    void* payload_raw;   // message content as raw bytes, optional for register/unregister
    size_t size;
    bool reliability;
};
