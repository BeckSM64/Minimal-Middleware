#include <string>
#include <cstdint>
#include "IMmwMessageSerializer.h"
#include "MmwMessage.h"

class CerealSerializer : public IMmwMessageSerializer {
    public:
        std::string serialize(const MmwMessage& msg) override;
        std::string serialize_raw(const MmwMessage& msg) override;
        MmwMessage deserialize(const std::string& data) override;
        MmwMessage deserialize_raw(const std::string& data) override;
};
