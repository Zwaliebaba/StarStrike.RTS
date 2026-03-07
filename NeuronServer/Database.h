#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

// Forward declare libpq types to avoid including libpq-fe.h in the header.
struct pg_conn;
typedef struct pg_conn PGconn;

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

/// RAII wrapper for a single PostgreSQL connection.
class PgConnection
{
public:
    PgConnection() = default;
    ~PgConnection();

    PgConnection(const PgConnection&) = delete;
    PgConnection& operator=(const PgConnection&) = delete;
    PgConnection(PgConnection&& other) noexcept;
    PgConnection& operator=(PgConnection&& other) noexcept;

    bool connect(const std::string& connString);
    void disconnect();

    [[nodiscard]] bool isConnected() const noexcept;

    /// Execute a non-returning statement (INSERT, UPDATE, DELETE, DDL).
    bool execute(const std::string& sql);

    /// Execute a query and return rows.
    std::optional<std::vector<Row>> query(const std::string& sql);

private:
    PGconn* m_conn = nullptr;
};

/// Thread-safe connection pool for PostgreSQL.
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
    PgConnection* acquire();
    /// Return a connection to the pool.
    void release(PgConnection* conn);

    std::vector<std::unique_ptr<PgConnection>> m_pool;
    std::queue<PgConnection*>                  m_available;
    std::mutex                                 m_mutex;
    bool                                       m_connected = false;
    DatabaseConfig                             m_config;
};

} // namespace Neuron::Server
