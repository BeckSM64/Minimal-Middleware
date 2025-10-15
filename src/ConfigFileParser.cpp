#include <yaml-cpp/yaml.h>
#include <iostream>
#include "ConfigFileParser.h"

bool ConfigFileParser::parseConfigFile(const std::string &filename) {
    try {
        YAML::Node config = YAML::LoadFile(filename);
        if (config["broker"]) {
            brokerIp = config["broker"]["ip"].as<std::string>();
            brokerPort = config["broker"]["port"].as<int>();
            std::cout << "Successfully parsed config file\n"
                << "Broker IP: "
                << brokerIp
                << "\n"
                << "Broker Port: "
                << brokerPort
                << "\n"
                << std::endl;
        }
    } catch (const YAML::Exception&e) {
        std::cerr << "Failed to load config: " << e.what() << "\n";
        return false;
    }
    return true;
}

std::string ConfigFileParser::getBrokerIp() {
    return brokerIp;
}

int ConfigFileParser::getBrokerPort() {
    return brokerPort;
}
