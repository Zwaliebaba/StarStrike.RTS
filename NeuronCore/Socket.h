#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Neuron
{

/// Received datagram: payload bytes + sender address string ("ip:port").
struct Datagram
{
    std::vector<uint8_t> data;
    std::string          senderAddr;
    uint16_t             senderPort = 0;
};

/// Cross-platform non-blocking UDP socket.
///
/// Windows: WinSock2 (ws2_32.lib)
/// Linux/macOS: POSIX BSD sockets
class UDPSocket
{
public:
    UDPSocket() = default;
    ~UDPSocket(); // releases socket

    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    UDPSocket(UDPSocket&& other) noexcept;
    UDPSocket& operator=(UDPSocket&& other) noexcept;

    /// Bind to a local address and port. Returns true on success.
    bool bind(const std::string& addr, uint16_t port);

    /// Send a datagram to a remote address:port. Returns true on success.
    bool sendTo(std::span<const uint8_t> data,
                const std::string& addr, uint16_t port);

    /// Non-blocking receive. Returns nullopt if no data available.
    std::optional<Datagram> recvFrom();

    /// Close the socket explicitly (also called by destructor).
    void close();

    [[nodiscard]] bool isOpen() const noexcept { return m_open; }

    /// Return the local port the socket is bound to (useful after bind to port 0).
    [[nodiscard]] uint16_t localPort() const;

private:
#ifdef _WIN32
    uintptr_t m_sock = ~static_cast<uintptr_t>(0); // INVALID_SOCKET
#else
    int       m_sock = -1;
#endif
    bool m_open = false;
};

} // namespace Neuron
