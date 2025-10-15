#pragma once
#include <string>
#include "MmwMessage.h"

class IMmwMessageSerializer {
    public:
        virtual ~IMmwMessageSerializer() {}
        virtual std::string serialize(const MmwMessage& msg) = 0;
        virtual MmwMessage deserialize(const std::string& data) = 0;
};
