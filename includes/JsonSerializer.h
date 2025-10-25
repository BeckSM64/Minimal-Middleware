#include <nlohmann/json.hpp>
#include "IMmwMessageSerializer.h"
#include "MmwMessage.h"

class JsonSerializer : public IMmwMessageSerializer {
    public:
        std::string serialize(const MmwMessage& msg) override;
        std::string serialize_raw(const MmwMessage& msg) override;
        MmwMessage deserialize(const std::string& data) override;
        MmwMessage deserialize_raw(const std::string& data) override;
};
