#include "CerealSerializer.h"
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <sstream>
#include <cstring>

std::string CerealSerializer::serialize(const MmwMessage& msg) {
    std::ostringstream oss(std::ios::binary);
    {
        cereal::BinaryOutputArchive ar(oss);
        ar(msg.messageId, msg.type, msg.topic, msg.payload, msg.reliability);
    }
    return oss.str();
}

std::string CerealSerializer::serialize_raw(const MmwMessage& msg) {
    std::ostringstream oss(std::ios::binary);
    {
        cereal::BinaryOutputArchive ar(oss);

        // Copy raw payload into a vector for serialization
        std::vector<unsigned char> bytes(
            static_cast<const unsigned char*>(msg.payload_raw),
            static_cast<const unsigned char*>(msg.payload_raw) + msg.size
        );

        ar(msg.messageId, msg.type, msg.topic, bytes, msg.reliability);
    }
    return oss.str();
}

MmwMessage CerealSerializer::deserialize(const std::string& data) {
    MmwMessage msg;
    std::istringstream iss(data, std::ios::binary);
    {
        cereal::BinaryInputArchive ar(iss);
        ar(msg.messageId, msg.type, msg.topic, msg.payload, msg.reliability);
    }

    msg.size = msg.payload.size();
    msg.payload_raw = malloc(msg.size);
    memcpy(msg.payload_raw, msg.payload.data(), msg.size);

    return msg;
}

MmwMessage CerealSerializer::deserialize_raw(const std::string& data) {
    MmwMessage msg;
    std::istringstream iss(data, std::ios::binary);
    {
        cereal::BinaryInputArchive ar(iss);
        std::vector<unsigned char> bytes;
        ar(msg.messageId, msg.type, msg.topic, bytes, msg.reliability);

        msg.size = bytes.size();
        msg.payload_raw = malloc(msg.size);
        memcpy(msg.payload_raw, bytes.data(), msg.size);

        msg.payload.assign(reinterpret_cast<const char*>(bytes.data()), msg.size);
    }
    return msg;
}
