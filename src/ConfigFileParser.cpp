#include <yaml-cpp/yaml.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "ConfigFileParser.h"

bool ConfigFileParser::parseConfigFile(const std::string &filename) {
    try {
        YAML::Node config = YAML::LoadFile(filename);
        if (config["broker"]) {
            brokerIp = config["broker"]["ip"].as<std::string>();
            brokerPort = config["broker"]["port"].as<int>();
            reliability = config["broker"]["reliability"].as<std::string>();
            spdlog::info("Successfully parsed config file");
            spdlog::info("Broker IP:{}", brokerIp);
            spdlog::info("Broker Port:{}", brokerPort);
        }
    } catch (const YAML::Exception&e) {
        spdlog::error("Failed to load config: {}", e.what());
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

std::string ConfigFileParser::getBrokerReliability() {
    return reliability;
}
