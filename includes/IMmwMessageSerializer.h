#pragma once
#include <string>
#include "MmwMessage.h"

class IMmwMessageSerializer {
    public:
        virtual ~IMmwMessageSerializer() {}
        virtual std::string serialize(const MmwMessage& msg) = 0;
        virtual std::string serialize_raw(const MmwMessage& msg) = 0;
        virtual MmwMessage deserialize(const std::string& data) = 0;
        virtual MmwMessage deserialize_raw(const std::string& data) = 0;
};
