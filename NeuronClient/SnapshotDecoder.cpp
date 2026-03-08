#include "pch.h"
#include "SnapshotDecoder.h"

#include <cstring>

namespace Neuron::Client
{

std::optional<SnapshotState> decodeSnapshot(
    uint8_t packetType,
    std::span<const uint8_t> payload)
{
    if (packetType != SnapState::TYPE)
        return std::nullopt;

    if (payload.size() < sizeof(SnapState))
        return std::nullopt;

    SnapState snap;
    std::memcpy(&snap, payload.data(), sizeof(SnapState));

    if (snap.entityCount > SnapState::MAX_ENTITIES_PER_SNAP)
        return std::nullopt;

    SnapshotState result;
    result.serverTick = snap.serverTick;
    result.entities.reserve(snap.entityCount);

    for (uint16_t i = 0; i < snap.entityCount; ++i)
        result.entities.push_back(snap.entities[i]);

    return result;
}

} // namespace Neuron::Client
