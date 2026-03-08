#pragma once

#include "PacketTypes.h"
#include "Types.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Neuron::Client
{

/// Delta for a single voxel change received from the server.
struct VoxelDelta
{
    Vec3i   pos  = {};
    uint8_t type = 0;
};

/// Decoded snapshot state from the server.
struct SnapshotState
{
    uint64_t                   serverTick   = 0;
    std::vector<SnapEntityData> entities;
    std::vector<VoxelDelta>     voxelDeltas;
};

/// Decode a SnapState packet payload into a SnapshotState.
/// Returns nullopt if the payload is malformed or the packet type doesn't match.
[[nodiscard]] std::optional<SnapshotState> decodeSnapshot(
    uint8_t packetType,
    std::span<const uint8_t> payload);

} // namespace Neuron::Client
