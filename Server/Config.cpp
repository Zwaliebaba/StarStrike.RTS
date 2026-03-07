#include "pch.h"
#include "Config.h"

#include "ServerLog.h"

#include <filesystem>

namespace Neuron::Server
{

ServerConfig loadConfig(const std::string& path)
{
    ServerConfig cfg;

    if (!std::filesystem::exists(path))
    {
        LogWarn("Config file not found: {} — using defaults\n", path);
        return cfg;
    }

    try
    {
        YAML::Node root = YAML::LoadFile(path);

        if (auto server = root["server"])
        {
            if (server["port"])          cfg.port        = server["port"].as<uint16_t>();
            if (server["bind_address"])  cfg.bindAddress = server["bind_address"].as<std::string>();
            if (server["tick_rate_hz"])  cfg.tickRateHz  = server["tick_rate_hz"].as<uint32_t>();
            if (server["max_players"])   cfg.maxPlayers  = server["max_players"].as<uint32_t>();
        }

        if (auto db = root["database"])
        {
            if (db["url"])       cfg.databaseUrl = db["url"].as<std::string>();
            if (db["pool_size"]) cfg.dbPoolSize  = db["pool_size"].as<int>();
        }

        if (auto logging = root["logging"])
        {
            if (logging["level"]) cfg.logLevel = logging["level"].as<std::string>();
        }

        LogInfo("Configuration loaded from {}\n", path);
    }
    catch (const YAML::Exception& e)
    {
        LogFatal("Failed to parse config {}: {}", path, e.what());
    }

    return cfg;
}

void initLogging(const std::string& /*level*/)
{
}

} // namespace Neuron::Server
