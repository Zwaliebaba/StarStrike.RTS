#pragma once

#include "PacketTypes.h"
#include "Types.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Neuron::Client
{

/// Client-side entity representation (mirrored from server snapshots).
struct ClientEntity
{
    EntityID   id              = INVALID_ENTITY;
    Vec3       pos             = {};
    Vec3       vel             = {};
    EntityType type            = EntityType::Ship;
    float      health          = 0.0f;
    PlayerID   ownerId         = INVALID_PLAYER;
    uint64_t   lastSnapshotTick = 0;

    // Interpolation targets (from latest snapshot)
    Vec3       targetPos       = {};
    Vec3       prevPos         = {};
};

/// Maintains a client-side cache of all known entities,
/// updated from server snapshots. Supports linear interpolation
/// for smooth rendering between snapshot updates.
class EntityCache
{
public:
    EntityCache() = default;

    /// Apply entity data from a server snapshot.
    void updateFromSnapshot(uint64_t serverTick,
                            const SnapEntityData* entities,
                            uint16_t count);

    /// Interpolate entity positions for smooth rendering.
    /// @param alpha blend factor in [0, 1] between previous and target positions.
    void interpolate(float alpha);

    /// Remove all entities.
    void clear();

    /// Read-only access to all cached entities.
    [[nodiscard]] const std::vector<ClientEntity>& getAll() const noexcept { return m_entities; }

    /// Look up a single entity by ID. Returns nullptr if not found.
    [[nodiscard]] const ClientEntity* getEntity(EntityID id) const;

    /// Number of cached entities.
    [[nodiscard]] size_t count() const noexcept { return m_entities.size(); }

    /// Server tick of the last applied snapshot.
    [[nodiscard]] uint64_t lastTick() const noexcept { return m_lastTick; }

private:
    std::vector<ClientEntity>                   m_entities;
    std::unordered_map<EntityID, size_t>        m_lookup;
    uint64_t                                    m_lastTick = 0;
};

} // namespace Neuron::Client
