#include <nlohmann/json.hpp>
#include <sstream>
#include "JsonSerializer.h"
#include <stdexcept>
#include <cctype>

std::string to_hex(const void* data, size_t size) {
    auto bytes = static_cast<const unsigned char*>(data);
    std::ostringstream oss;
    for (size_t i = 0; i < size; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    return oss.str();
}

std::vector<unsigned char> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0)
        throw std::runtime_error("Invalid hex length");

    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned char byte = (std::stoi(hex.substr(i, 2), nullptr, 16) & 0xFF);
        bytes.push_back(byte);
    }
    return bytes;
}

std::string JsonSerializer::serialize(const MmwMessage& msg) {
    nlohmann::json j;
    j["messageId"] = std::to_string(msg.messageId);
    j["type"] = msg.type;
    j["topic"] = msg.topic;
    j["payload"] = msg.payload;
    return j.dump();
}

std::string JsonSerializer::serialize_raw(const MmwMessage& msg) {
    nlohmann::json j;
    j["type"] = msg.type;
    j["topic"] = msg.topic;
    j["payload"] = to_hex(msg.payload_raw, msg.size);
    return j.dump();
}

MmwMessage JsonSerializer::deserialize(const std::string& data) {
    MmwMessage msg;
    auto j = nlohmann::json::parse(data);
    msg.messageId = std::stoul(j.value("messageId", ""));
    msg.type = j.value("type", "");
    msg.topic = j.value("topic", "");
    msg.payload = j.value("payload", "");
    return msg;
}

MmwMessage JsonSerializer::deserialize_raw(const std::string& data) {
    MmwMessage msg;
    auto j = nlohmann::json::parse(data);

    msg.type = j.value("type", "");
    msg.topic = j.value("topic", "");

    std::string payloadHex = j.value("payload", "");
    std::vector<unsigned char> bytes = from_hex(payloadHex);

    msg.size = bytes.size();
    msg.payload_raw = malloc(msg.size);
    memcpy(msg.payload_raw, bytes.data(), msg.size);

    // optional: also store as string
    msg.payload.assign(reinterpret_cast<char*>(bytes.data()), msg.size);

    return msg;
}


