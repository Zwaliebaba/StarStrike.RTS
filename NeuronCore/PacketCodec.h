#pragma once

#include "PacketTypes.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Neuron
{

// ── CRC-32C ─────────────────────────────────────────────────────────────────
// Uses SSE4.2 intrinsic (_mm_crc32_u8) when available, software fallback otherwise.

uint32_t crc32c(std::span<const uint8_t> data) noexcept;

// ── Packet Codec ────────────────────────────────────────────────────────────
// Encodes/decodes the packet wire format defined in PacketTypes.h.
//
// Encode: payload → [PacketHeader | payload bytes]
// Decode: raw bytes → validated header + payload span

enum class DecodeResult : uint8_t
{
    Ok,
    TooShort,
    BadMagic,
    BadCrc,
    Oversized
};

struct DecodedPacket
{
    PacketHeader              header;
    std::vector<uint8_t>      payload;
};

/// Encode a typed payload into a wire-format packet.
/// Returns the complete byte buffer ready to send.
template <typename T>
std::vector<uint8_t> encodePacket(const T& payload, uint32_t sequence)
{
    const auto* raw = reinterpret_cast<const uint8_t*>(&payload);
    uint16_t size   = static_cast<uint16_t>(sizeof(T));

    PacketHeader hdr;
    hdr.type        = T::TYPE;
    hdr.sequence    = sequence;
    hdr.payloadSize = size;
    hdr.crc         = crc32c({ raw, size });

    std::vector<uint8_t> buf;
    buf.resize(PACKET_HEADER_SIZE + size);

    // Write header fields (little-endian on x64 MSVC)
    auto* p = buf.data();
    std::memcpy(p,      &hdr.magic,       4); p += 4;
    std::memcpy(p,      &hdr.type,        1); p += 1;
    std::memcpy(p,      &hdr.flags,       1); p += 1;
    std::memcpy(p,      &hdr.reserved,    2); p += 2;
    std::memcpy(p,      &hdr.sequence,    4); p += 4;
    std::memcpy(p,      &hdr.payloadSize, 2); p += 2;
    std::memcpy(p,      &hdr.crc,         4); p += 4;

    // Write payload
    std::memcpy(p, raw, size);
    return buf;
}

/// Decode raw bytes into a validated packet.
DecodeResult decodePacket(std::span<const uint8_t> raw, DecodedPacket& out);

} // namespace Neuron
