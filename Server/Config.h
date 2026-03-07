#pragma once

#include <cstdint>
#include <string>

namespace Neuron::Server
{

/// Server configuration loaded from YAML.
struct ServerConfig
{
    // Network
    uint16_t    port        = 7777;
    std::string bindAddress = "0.0.0.0";

    // Simulation
    uint32_t    tickRateHz  = 60;
    uint32_t    maxPlayers  = 50;

    // Database
    std::string databaseUrl = "postgres://localhost:5432/starstrike";
    int         dbPoolSize  = 4;

    // Logging
    std::string logLevel    = "info";
};

/// Load server configuration from a YAML file.
/// Falls back to defaults if the file cannot be read.
ServerConfig loadConfig(const std::string& path);

/// Initialize logging (currently a no-op; server uses ServerLog.h stdout/stderr).
void initLogging(const std::string& level);

} // namespace Neuron::Server
