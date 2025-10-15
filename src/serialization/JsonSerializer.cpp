#include <nlohmann/json.hpp>
#include "JsonSerializer.h"

std::string JsonSerializer::serialize(const MmwMessage& msg) {
    nlohmann::json j;
    j["type"] = msg.type;
    j["topic"] = msg.topic;
    j["payload"] = msg.payload;
    return j.dump();
}

MmwMessage JsonSerializer::deserialize(const std::string& data) {
    MmwMessage msg;
    auto j = nlohmann::json::parse(data);
    msg.type = j.value("type", "");
    msg.topic = j.value("topic", "");
    msg.payload = j.value("payload", "");
    return msg;
}
