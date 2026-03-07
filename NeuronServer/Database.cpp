#include "pch.h"
#include "Database.h"

#include "ServerLog.h"

#include <sql.h>
#include <sqlext.h>

namespace Neuron::Server
{

// ── Helper: extract ODBC diagnostic message ─────────────────────────────────

static std::string getOdbcError(SQLSMALLINT handleType, SQLHANDLE handle)
{
    SQLCHAR     sqlState[6]{};
    SQLINTEGER  nativeError = 0;
    SQLCHAR     message[512]{};
    SQLSMALLINT msgLen = 0;

    if (SQLGetDiagRecA(handleType, handle, 1, sqlState, &nativeError,
                       message, sizeof(message), &msgLen) == SQL_SUCCESS)
    {
        return std::string(reinterpret_cast<char*>(message), msgLen);
    }
    return "Unknown ODBC error";
}

// ── SqlConnection ───────────────────────────────────────────────────────────

SqlConnection::~SqlConnection()
{
    disconnect();
}

SqlConnection::SqlConnection(SqlConnection&& other) noexcept
    : m_hdbc(other.m_hdbc)
{
    other.m_hdbc = nullptr;
}

SqlConnection& SqlConnection::operator=(SqlConnection&& other) noexcept
{
    if (this != &other)
    {
        disconnect();
        m_hdbc = other.m_hdbc;
        other.m_hdbc = nullptr;
    }
    return *this;
}

bool SqlConnection::connect(SQLHENV hEnv, const std::string& connString)
{
    disconnect();

    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &m_hdbc);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        LogError("ODBC: failed to allocate connection handle\n");
        m_hdbc = nullptr;
        return false;
    }

    // Set login timeout
    SQLSetConnectAttrA(static_cast<SQLHDBC>(m_hdbc), SQL_LOGIN_TIMEOUT,
                       reinterpret_cast<SQLPOINTER>(10), SQL_IS_INTEGER);

    SQLCHAR outConn[1024]{};
    SQLSMALLINT outLen = 0;
    ret = SQLDriverConnectA(static_cast<SQLHDBC>(m_hdbc), nullptr,
                            reinterpret_cast<SQLCHAR*>(const_cast<char*>(connString.c_str())),
                            static_cast<SQLSMALLINT>(connString.size()),
                            outConn, sizeof(outConn), &outLen,
                            SQL_DRIVER_NOPROMPT);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        LogError("SQL Server connect failed: {}\n",
                 getOdbcError(SQL_HANDLE_DBC, m_hdbc));
        SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
        m_hdbc = nullptr;
        return false;
    }

    LogInfo("SQL Server connection established\n");
    return true;
}

void SqlConnection::disconnect()
{
    if (m_hdbc)
    {
        SQLDisconnect(static_cast<SQLHDBC>(m_hdbc));
        SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
        m_hdbc = nullptr;
    }
}

bool SqlConnection::isConnected() const noexcept
{
    if (!m_hdbc) return false;

    // Check connection status via ODBC attribute
    SQLUINTEGER deadConn = SQL_CD_FALSE;
    SQLRETURN ret = SQLGetConnectAttrA(
        static_cast<SQLHDBC>(const_cast<void*>(m_hdbc)),
        SQL_ATTR_CONNECTION_DEAD, &deadConn, 0, nullptr);
    return (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
           && deadConn == SQL_CD_FALSE;
}

bool SqlConnection::execute(const std::string& sql)
{
    if (!m_hdbc) return false;

    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hStmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
        return false;

    auto start = std::chrono::steady_clock::now();
    ret = SQLExecDirectA(hStmt,
                         reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())),
                         SQL_NTS);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    if (ms > 100)
        LogWarn("Slow query ({} ms): {}\n", ms, sql.substr(0, 120));

    bool ok = (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO || ret == SQL_NO_DATA);
    if (!ok)
        LogError("SQL execute failed: {}\n", getOdbcError(SQL_HANDLE_STMT, hStmt));

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return ok;
}

std::optional<std::vector<Row>> SqlConnection::query(const std::string& sql)
{
    if (!m_hdbc) return std::nullopt;

    SQLHSTMT hStmt = nullptr;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &hStmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
        return std::nullopt;

    auto start = std::chrono::steady_clock::now();
    ret = SQLExecDirectA(hStmt,
                         reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql.c_str())),
                         SQL_NTS);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    if (ms > 100)
        LogWarn("Slow query ({} ms): {}\n", ms, sql.substr(0, 120));

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        LogError("SQL query failed: {}\n", getOdbcError(SQL_HANDLE_STMT, hStmt));
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return std::nullopt;
    }

    SQLSMALLINT cols = 0;
    SQLNumResultCols(hStmt, &cols);

    std::vector<Row> result;
    while (SQLFetch(hStmt) == SQL_SUCCESS)
    {
        Row row;
        row.reserve(static_cast<size_t>(cols));
        for (SQLSMALLINT c = 1; c <= cols; ++c)
        {
            SQLCHAR buf[4096]{};
            SQLLEN indicator = 0;
            SQLGetData(hStmt, c, SQL_C_CHAR, buf, sizeof(buf), &indicator);
            if (indicator == SQL_NULL_DATA)
                row.emplace_back();
            else
                row.emplace_back(reinterpret_cast<char*>(buf));
        }
        result.push_back(std::move(row));
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
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

    // Allocate ODBC environment handle
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO)
    {
        LogError("ODBC: failed to allocate environment handle\n");
        return false;
    }
    SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION,
                  reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3_80), 0);

    int successCount = 0;
    m_pool.reserve(static_cast<size_t>(config.poolSize));

    for (int i = 0; i < config.poolSize; ++i)
    {
        auto conn = std::make_unique<SqlConnection>();
        if (conn->connect(m_hEnv, config.connectionString))
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
    // SqlConnection destructors handle SQLDisconnect/SQLFreeHandle
    m_pool.clear();
    while (!m_available.empty())
        m_available.pop();
    m_connected = false;

    if (m_hEnv)
    {
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hEnv = nullptr;
    }
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
    return execute("BEGIN TRANSACTION");
}

bool Database::commit()
{
    return execute("COMMIT");
}

SqlConnection* Database::acquire()
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

void Database::release(SqlConnection* conn)
{
    std::lock_guard lock(m_mutex);
    m_available.push(conn);
}

} // namespace Neuron::Server
