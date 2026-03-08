#pragma once

#include "Socket.h"
#include "PacketCodec.h"
#include "PacketTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Neuron::Client
{

/// Client-side UDP socket wrapper.
/// Connects to a server, sends commands, and receives snapshots (non-blocking).
class ClientSocket
{
public:
    ClientSocket() = default;

    /// Connect to a server at the given address and port.
    /// Binds a local socket and stores the server endpoint for sending.
    bool connect(const std::string& serverAddr, uint16_t serverPort);

    /// Disconnect and close the socket.
    void disconnect();

    /// Send a typed packet (CmdInput, CmdChat, etc.) to the server.
    template <typename T>
    bool sendCommand(const T& cmd)
    {
        auto bytes = encodePacket(cmd, m_sendSequence++);
        return m_socket.sendTo(bytes, m_serverAddr, m_serverPort);
    }

    /// Non-blocking receive. Returns decoded packet if data available.
    std::optional<DecodedPacket> recv();

    [[nodiscard]] bool isConnected() const noexcept { return m_connected; }
    [[nodiscard]] uint32_t sendSequence() const noexcept { return m_sendSequence; }

private:
    UDPSocket   m_socket;
    std::string m_serverAddr;
    uint16_t    m_serverPort  = 0;
    uint32_t    m_sendSequence = 0;
    bool        m_connected   = false;
};

} // namespace Neuron::Client
