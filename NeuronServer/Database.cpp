#include "pch.h"
#include "Database.h"

#include "ServerLog.h"

#include <libpq-fe.h>

namespace Neuron::Server
{

// ── PgConnection ────────────────────────────────────────────────────────────

PgConnection::~PgConnection()
{
    disconnect();
}

PgConnection::PgConnection(PgConnection&& other) noexcept
    : m_conn(other.m_conn)
{
    other.m_conn = nullptr;
}

PgConnection& PgConnection::operator=(PgConnection&& other) noexcept
{
    if (this != &other)
    {
        disconnect();
        m_conn = other.m_conn;
        other.m_conn = nullptr;
    }
    return *this;
}

bool PgConnection::connect(const std::string& connString)
{
    disconnect();
    m_conn = PQconnectdb(connString.c_str());

    if (PQstatus(m_conn) != CONNECTION_OK)
    {
        LogError("PostgreSQL connect failed: {}\n", PQerrorMessage(m_conn));
        PQfinish(m_conn);
        m_conn = nullptr;
        return false;
    }

    LogInfo("PostgreSQL connection established\n");
    return true;
}

void PgConnection::disconnect()
{
    if (m_conn)
    {
        PQfinish(m_conn);
        m_conn = nullptr;
    }
}

bool PgConnection::isConnected() const noexcept
{
    return m_conn != nullptr && PQstatus(m_conn) == CONNECTION_OK;
}

bool PgConnection::execute(const std::string& sql)
{
    if (!m_conn) return false;

    auto start = std::chrono::steady_clock::now();
    PGresult* res = PQexec(m_conn, sql.c_str());
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    if (ms > 100)
        LogWarn("Slow query ({} ms): {}\n", ms, sql.substr(0, 120));

    ExecStatusType status = PQresultStatus(res);
    bool ok = (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK);
    if (!ok)
        LogError("SQL execute failed: {}\n", PQerrorMessage(m_conn));

    PQclear(res);
    return ok;
}

std::optional<std::vector<Row>> PgConnection::query(const std::string& sql)
{
    if (!m_conn) return std::nullopt;

    auto start = std::chrono::steady_clock::now();
    PGresult* res = PQexec(m_conn, sql.c_str());
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    if (ms > 100)
        LogWarn("Slow query ({} ms): {}\n", ms, sql.substr(0, 120));

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        LogError("SQL query failed: {}\n", PQerrorMessage(m_conn));
        PQclear(res);
        return std::nullopt;
    }

    int rows = PQntuples(res);
    int cols = PQnfields(res);
    std::vector<Row> result;
    result.reserve(static_cast<size_t>(rows));

    for (int r = 0; r < rows; ++r)
    {
        Row row;
        row.reserve(static_cast<size_t>(cols));
        for (int c = 0; c < cols; ++c)
            row.emplace_back(PQgetvalue(res, r, c));
        result.push_back(std::move(row));
    }

    PQclear(res);
    return result;
}

// ── Database (Connection Pool) ──────────────────────────────────────────────

Database::~Database()
{
    disconnect();
}

bool Database::connect(const DatabaseConfig& config)
{
    std::lock_guard lock(m_mutex);
    m_config = config;

    int successCount = 0;
    m_pool.reserve(static_cast<size_t>(config.poolSize));

    for (int i = 0; i < config.poolSize; ++i)
    {
        auto conn = std::make_unique<PgConnection>();
        if (conn->connect(config.connectionString))
        {
            m_available.push(conn.get());
            m_pool.push_back(std::move(conn));
            ++successCount;
        }
        else
        {
            LogWarn("Pool connection {}/{} failed\n", i + 1, config.poolSize);
        }
    }

    m_connected = successCount > 0;
    LogInfo("Database pool: {}/{} connections established\n",
            successCount, config.poolSize);
    return m_connected;
}

void Database::disconnect()
{
    std::lock_guard lock(m_mutex);
    // PgConnection destructors handle PQfinish
    m_pool.clear();
    while (!m_available.empty())
        m_available.pop();
    m_connected = false;
}

bool Database::execute(const std::string& sql)
{
    auto* conn = acquire();
    if (!conn) return false;

    bool ok = conn->execute(sql);
    release(conn);
    return ok;
}

std::optional<std::vector<Row>> Database::query(const std::string& sql)
{
    auto* conn = acquire();
    if (!conn) return std::nullopt;

    auto result = conn->query(sql);
    release(conn);
    return result;
}

bool Database::beginTransaction()
{
    return execute("BEGIN");
}

bool Database::commit()
{
    return execute("COMMIT");
}

PgConnection* Database::acquire()
{
    std::lock_guard lock(m_mutex);
    if (m_available.empty())
    {
        LogWarn("No database connections available in pool\n");
        return nullptr;
    }
    auto* conn = m_available.front();
    m_available.pop();
    return conn;
}

void Database::release(PgConnection* conn)
{
    std::lock_guard lock(m_mutex);
    m_available.push(conn);
}

} // namespace Neuron::Server
