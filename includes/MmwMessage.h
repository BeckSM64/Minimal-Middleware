#pragma once
#include <string>

struct MmwMessage {
    std::string type;    // "PUB_REGISTER", "SUB_REGISTER", "DATA", "UNREGISTER"
    std::string topic;   // topic name
    std::string payload; // message content, optional for register/unregister
};
