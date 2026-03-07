#include "pch.h"
#include "SocketManager.h"

#include "Constants.h"
#include "ServerLog.h"

#include <format>

namespace Neuron::Server
{

bool SocketManager::init(const std::string& bindAddr, uint16_t port)
{
    if (!m_socket.bind(bindAddr, port))
    {
        LogError("SocketManager: failed to bind {}:{}\n", bindAddr, port);
        return false;
    }

    LogInfo("SocketManager: listening on {}:{}/udp\n", bindAddr, port);
    return true;
}

void SocketManager::recvPackets(std::vector<ReceivedPacket>& outPackets)
{
    for (uint32_t i = 0; i < Neuron::MAX_RECV_PER_TICK; ++i)
    {
        auto dg = m_socket.recvFrom();
        if (!dg.has_value())
            break; // No more data this tick

        // Validate and decode the packet
        DecodedPacket decoded;
        DecodeResult result = decodePacket(dg->data, decoded);

        switch (result)
        {
        case DecodeResult::Ok:
        {
            auto key = makeSenderKey(dg->senderAddr, dg->senderPort);

            // Duplicate detection — drop if sequence already seen
            if (isDuplicate(key, decoded.header.sequence))
                break;

            outPackets.push_back({
                .packet     = std::move(decoded),
                .senderAddr = std::move(dg->senderAddr),
                .senderPort = dg->senderPort
            });
            break;
        }

        case DecodeResult::BadMagic:
            // Drop silently — non-StarStrike traffic
            break;

        case DecodeResult::BadCrc:
        {
            auto key = makeSenderKey(dg->senderAddr, dg->senderPort);
            if (shouldLogCrcError(key))
                LogWarn("Bad CRC from {} (suppressing further logs for 1 min)\n", key);
            break;
        }

        case DecodeResult::Oversized:
            LogWarn("Oversized packet from {}:{} ({} bytes)\n",
                       dg->senderAddr, dg->senderPort, dg->data.size());
            break;

        case DecodeResult::TooShort:
            LogWarn("Truncated packet from {}:{} ({} bytes)\n",
                       dg->senderAddr, dg->senderPort, dg->data.size());
            break;
        }
    }
}

bool SocketManager::sendTo(std::span<const uint8_t> data,
                           const std::string& addr, uint16_t port)
{
    return m_socket.sendTo(data, addr, port);
}

// ── Private helpers ─────────────────────────────────────────────────────────

std::string SocketManager::makeSenderKey(const std::string& addr, uint16_t port)
{
    return std::format("{}:{}", addr, port);
}

bool SocketManager::isDuplicate(const std::string& senderKey, uint32_t sequence)
{
    auto& state = m_senderStates[senderKey];

    // Check if this sequence number was already seen recently
    for (uint32_t seq : state.recentSequences)
    {
        if (seq == sequence)
            return true;
    }

    // Track the new sequence number
    state.recentSequences.push_back(sequence);
    if (state.recentSequences.size() > DEDUP_WINDOW)
        state.recentSequences.pop_front();

    return false;
}

bool SocketManager::shouldLogCrcError(const std::string& senderKey)
{
    auto now = std::chrono::steady_clock::now();
    auto& state = m_senderStates[senderKey];

    if (now - state.lastCrcLogTime >= std::chrono::minutes(1))
    {
        state.lastCrcLogTime = now;
        return true;
    }

    return false;
}

} // namespace Neuron::Server
