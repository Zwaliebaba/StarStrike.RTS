#pragma once

#include "Socket.h"
#include "PacketCodec.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace Neuron::Server
{

/// A received and validated packet with sender info.
struct ReceivedPacket
{
    DecodedPacket packet;
    std::string   senderAddr;
    uint16_t      senderPort = 0;
};

/// Manages the server's UDP socket: binds, receives, validates,
/// deduplicates, and enqueues packets for the simulation tick loop.
class SocketManager
{
public:
    SocketManager() = default;

    /// Bind the socket to the given address and port.
    bool init(const std::string& bindAddr, uint16_t port);

    /// Non-blocking: receive up to MAX_RECV_PER_TICK datagrams,
    /// validate them (magic, CRC, dedup), and append to outPackets.
    void recvPackets(std::vector<ReceivedPacket>& outPackets);

    /// Send a raw byte buffer to a specific client.
    bool sendTo(std::span<const uint8_t> data,
                const std::string& addr, uint16_t port);

    [[nodiscard]] bool isOpen() const noexcept { return m_socket.isOpen(); }

private:
    static constexpr uint32_t DEDUP_WINDOW = 60; // Track last 60 sequence numbers per sender

    /// Per-sender state for dedup and rate-limited logging.
    struct SenderState
    {
        std::deque<uint32_t>                      recentSequences;
        std::chrono::steady_clock::time_point     lastCrcLogTime{};
    };

    /// Build a string key "addr:port" for sender lookup.
    [[nodiscard]] static std::string makeSenderKey(const std::string& addr, uint16_t port);

    /// Returns true if this sequence was already seen from the sender.
    bool isDuplicate(const std::string& senderKey, uint32_t sequence);

    /// Returns true if a CRC error log is allowed for this sender (rate limited to 1/min).
    bool shouldLogCrcError(const std::string& senderKey);

    Neuron::UDPSocket                              m_socket;
    std::unordered_map<std::string, SenderState>   m_senderStates;
};

} // namespace Neuron::Server
