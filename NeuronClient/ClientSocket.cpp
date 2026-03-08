#include "pch.h"
#include "ClientSocket.h"

namespace Neuron::Client
{

bool ClientSocket::connect(const std::string& serverAddr, uint16_t serverPort)
{
    if (m_connected)
        disconnect();

    // Bind to any local port (0 = OS-assigned ephemeral port)
    if (!m_socket.bind("0.0.0.0", 0))
    {
        DebugTrace("ClientSocket: failed to bind local socket\n");
        return false;
    }

    m_serverAddr   = serverAddr;
    m_serverPort   = serverPort;
    m_sendSequence = 0;
    m_connected    = true;

    // Send an initial ping so the server registers us
    PingPacket ping;
    ping.serverTick  = 0;
    ping.serverTimeUs = 0;
    sendCommand(ping);

    DebugTrace("ClientSocket: connected to {}:{}\n", serverAddr, serverPort);
    return true;
}

void ClientSocket::disconnect()
{
    m_socket.close();
    m_connected = false;
    DebugTrace("ClientSocket: disconnected\n");
}

std::optional<DecodedPacket> ClientSocket::recv()
{
    auto datagram = m_socket.recvFrom();
    if (!datagram)
        return std::nullopt;

    DecodedPacket decoded;
    auto result = decodePacket(datagram->data, decoded);
    if (result != DecodeResult::Ok)
        return std::nullopt;

    return decoded;
}

} // namespace Neuron::Client
