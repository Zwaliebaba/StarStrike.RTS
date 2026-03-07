#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

// Forward declare ODBC handle types to avoid pulling in <sql.h> in the header.
using SQLHENV = void*;
using SQLHDBC = void*;

namespace Neuron::Server
{

/// Configuration for the database connection pool.
struct DatabaseConfig
{
    std::string connectionString;
    int         poolSize    = 4;
    int         idleTimeoutSec = 30;
    int         queryTimeoutMs = 1000;
};

/// A single row of query results (column values as strings).
using Row = std::vector<std::string>;

/// RAII wrapper for a single MS SQL Server ODBC connection.
class SqlConnection
{
public:
    SqlConnection() = default;
    ~SqlConnection();

    SqlConnection(const SqlConnection&) = delete;
    SqlConnection& operator=(const SqlConnection&) = delete;
    SqlConnection(SqlConnection&& other) noexcept;
    SqlConnection& operator=(SqlConnection&& other) noexcept;

    bool connect(SQLHENV hEnv, const std::string& connString);
    void disconnect();

    [[nodiscard]] bool isConnected() const noexcept;

    /// Execute a non-returning statement (INSERT, UPDATE, DELETE, DDL).
    bool execute(const std::string& sql);

    /// Execute a query and return rows.
    std::optional<std::vector<Row>> query(const std::string& sql);

private:
    SQLHDBC m_hdbc = nullptr;
};

/// Thread-safe connection pool for MS SQL Server via ODBC.
class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// Establish the connection pool. Returns true if at least one connection succeeded.
    bool connect(const DatabaseConfig& config);

    /// Close all connections.
    void disconnect();

    /// Execute a statement (write path).
    bool execute(const std::string& sql);

    /// Execute a query (read path).
    std::optional<std::vector<Row>> query(const std::string& sql);

    /// Transaction helpers — acquire a connection, begin, execute, commit/rollback.
    bool beginTransaction();
    bool commit();

    [[nodiscard]] bool isConnected() const noexcept { return m_connected; }

private:
    /// Borrow a connection from the pool (blocks if none available).
    SqlConnection* acquire();
    /// Return a connection to the pool.
    void release(SqlConnection* conn);

    SQLHENV                                     m_hEnv = nullptr;
    std::vector<std::unique_ptr<SqlConnection>> m_pool;
    std::queue<SqlConnection*>                  m_available;
    std::mutex                                  m_mutex;
    bool                                        m_connected = false;
    DatabaseConfig                              m_config;
};

} // namespace Neuron::Server
