#include "pch.h"
#include "PacketCodec.h"
#include "Constants.h"

#include <cstring>
#include <intrin.h>

namespace Neuron
{

// ── CRC-32C implementation ──────────────────────────────────────────────────

// Software fallback table (CRC-32C / Castagnoli polynomial 0x1EDC6F41)
namespace
{
    constexpr uint32_t CRC32C_POLY = 0x82F63B78u; // reflected

    consteval auto buildCrcTable()
    {
        std::array<uint32_t, 256> table{};
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j)
                crc = (crc >> 1) ^ (CRC32C_POLY & (~(crc & 1) + 1));
            table[i] = crc;
        }
        return table;
    }

    constexpr auto CRC_TABLE = buildCrcTable();
} // anonymous

uint32_t crc32c(std::span<const uint8_t> data) noexcept
{
    uint32_t crc = 0xFFFFFFFFu;

#ifdef _M_X64
    // SSE4.2 hardware CRC-32C
    const uint8_t* p = data.data();
    size_t len = data.size();

    // Process 8 bytes at a time
    while (len >= 8)
    {
        uint64_t val;
        std::memcpy(&val, p, 8);
        crc = static_cast<uint32_t>(_mm_crc32_u64(crc, val));
        p += 8;
        len -= 8;
    }
    // Process 4 bytes
    if (len >= 4)
    {
        uint32_t val;
        std::memcpy(&val, p, 4);
        crc = _mm_crc32_u32(crc, val);
        p += 4;
        len -= 4;
    }
    // Process remaining bytes
    while (len--)
        crc = _mm_crc32_u8(crc, *p++);
#else
    // Software fallback
    for (uint8_t b : data)
        crc = (crc >> 8) ^ CRC_TABLE[(crc ^ b) & 0xFF];
#endif

    return crc ^ 0xFFFFFFFFu;
}

// ── Packet Decode ───────────────────────────────────────────────────────────

DecodeResult decodePacket(std::span<const uint8_t> raw, DecodedPacket& out)
{
    if (raw.size() < PACKET_HEADER_SIZE)
        return DecodeResult::TooShort;

    const uint8_t* p = raw.data();

    // Read header fields
    std::memcpy(&out.header.magic,       p, 4); p += 4;
    std::memcpy(&out.header.type,        p, 1); p += 1;
    std::memcpy(&out.header.flags,       p, 1); p += 1;
    std::memcpy(&out.header.reserved,    p, 2); p += 2;
    std::memcpy(&out.header.sequence,    p, 4); p += 4;
    std::memcpy(&out.header.payloadSize, p, 2); p += 2;
    std::memcpy(&out.header.crc,         p, 4); p += 4;

    // Validate magic
    if (out.header.magic != PACKET_MAGIC)
        return DecodeResult::BadMagic;

    // Validate total size
    size_t expectedTotal = PACKET_HEADER_SIZE + out.header.payloadSize;
    if (expectedTotal > MAX_PACKET_SIZE)
        return DecodeResult::Oversized;
    if (raw.size() < expectedTotal)
        return DecodeResult::TooShort;

    // Extract payload
    auto payloadSpan = raw.subspan(PACKET_HEADER_SIZE, out.header.payloadSize);

    // Validate CRC over payload
    uint32_t computed = crc32c(payloadSpan);
    if (computed != out.header.crc)
        return DecodeResult::BadCrc;

    out.payload.assign(payloadSpan.begin(), payloadSpan.end());
    return DecodeResult::Ok;
}

} // namespace Neuron
