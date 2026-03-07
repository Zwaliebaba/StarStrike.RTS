#pragma once

#include "Types.h"
#include "Constants.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Neuron::GameLogic
{

/// An entity in the game world (ECS-lite, array-of-structs).
/// No pointers between entities — use EntityID + lookup map.
struct Entity
{
    EntityID   id             = INVALID_ENTITY;
    Vec3       pos            = {};
    Vec3       vel            = {};
    Vec3       accel          = {};
    Quat       rot            = {};
    EntityType type           = EntityType::Ship;
    PlayerID   ownerPlayerId  = INVALID_PLAYER;
    uint32_t   hp             = 0;
    uint32_t   maxHp          = 0;
    EntityID   targetId       = INVALID_ENTITY;
    uint64_t   lastUpdateTick = 0;
    bool       alive          = true;
};

/// ECS-lite entity manager with contiguous arrays for cache coherency.
/// Uses a free pool to reuse EntityIDs and avoid ID exhaustion.
class EntitySystem
{
public:
    EntitySystem() = default;

    /// Spawn a new entity. Returns the assigned EntityID.
    /// The entity's `id` field is set by the system (any caller-provided id is overwritten).
    [[nodiscard]] EntityID spawnEntity(const Entity& prototype);

    /// Destroy an entity by ID. The ID is returned to the free pool.
    void destroyEntity(EntityID id);

    /// Get a mutable pointer to an entity, or nullptr if not found / dead.
    [[nodiscard]] Entity* getEntity(EntityID id);

    /// Get a const pointer to an entity, or nullptr if not found / dead.
    [[nodiscard]] const Entity* getEntity(EntityID id) const;

    /// Read-only access to all entities (includes dead slots — check `alive`).
    [[nodiscard]] const std::vector<Entity>& getAll() const noexcept { return m_entities; }

    /// Mutable access to all entities (for tick phases that mutate in-place).
    [[nodiscard]] std::vector<Entity>& getAll() noexcept { return m_entities; }

    /// Number of living entities.
    [[nodiscard]] size_t liveCount() const noexcept { return m_liveCount; }

    /// Per-tick update: advance physics (pos += vel * dt, vel += accel * dt).
    void tickUpdate(float dt, uint64_t tickNum);

private:
    std::vector<Entity>                     m_entities;
    std::unordered_map<EntityID, size_t>    m_entityLookup;
    std::vector<EntityID>                   m_freePool;
    EntityID                                m_nextId    = 0;
    size_t                                  m_liveCount = 0;
};

} // namespace Neuron::GameLogic
