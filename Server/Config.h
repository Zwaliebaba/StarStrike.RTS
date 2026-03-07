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
    // Windows Auth: Trusted_Connection=yes (requires Windows login granted in SQL Server)
    // SQL Auth:     UID=sa;PWD=...;  (requires Mixed Mode enabled on instance)
    std::string databaseUrl = "Driver={ODBC Driver 18 for SQL Server};Server=localhost;Database=starstrike;Trusted_Connection=yes;Encrypt=optional;";
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
