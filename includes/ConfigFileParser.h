#pragma once

#include <string>

class ConfigFileParser {
    private:

        // Default values for broker
        std::string brokerIp = "127.0.0.1";
        int brokerPort = 5000;

    public:
        bool parseConfigFile(const std::string& filename);
        std::string getBrokerIp();
        int getBrokerPort();
};
