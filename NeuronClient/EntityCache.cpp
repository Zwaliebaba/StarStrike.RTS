#include "pch.h"
#include "EntityCache.h"

namespace Neuron::Client
{

void EntityCache::updateFromSnapshot(uint64_t serverTick,
                                     const SnapEntityData* entities,
                                     uint16_t count)
{
    m_lastTick = serverTick;

    for (uint16_t i = 0; i < count; ++i)
    {
        const auto& snap = entities[i];
        auto it = m_lookup.find(snap.entityId);

        if (it != m_lookup.end())
        {
            // Update existing entity
            auto& ce = m_entities[it->second];
            ce.prevPos         = ce.pos;
            ce.targetPos       = snap.position;
            ce.vel             = snap.velocity;
            ce.health          = snap.health;
            ce.ownerId         = snap.ownerId;
            ce.type            = snap.type;
            ce.lastSnapshotTick = serverTick;
        }
        else
        {
            // New entity
            ClientEntity ce;
            ce.id              = snap.entityId;
            ce.pos             = snap.position;
            ce.prevPos         = snap.position;
            ce.targetPos       = snap.position;
            ce.vel             = snap.velocity;
            ce.health          = snap.health;
            ce.ownerId         = snap.ownerId;
            ce.type            = snap.type;
            ce.lastSnapshotTick = serverTick;

            m_lookup[ce.id] = m_entities.size();
            m_entities.push_back(ce);
        }
    }
}

void EntityCache::interpolate(float alpha)
{
    for (auto& ce : m_entities)
    {
        ce.pos.x = ce.prevPos.x + (ce.targetPos.x - ce.prevPos.x) * alpha;
        ce.pos.y = ce.prevPos.y + (ce.targetPos.y - ce.prevPos.y) * alpha;
        ce.pos.z = ce.prevPos.z + (ce.targetPos.z - ce.prevPos.z) * alpha;
    }
}

void EntityCache::clear()
{
    m_entities.clear();
    m_lookup.clear();
    m_lastTick = 0;
}

const ClientEntity* EntityCache::getEntity(EntityID id) const
{
    auto it = m_lookup.find(id);
    if (it == m_lookup.end())
        return nullptr;
    return &m_entities[it->second];
}

} // namespace Neuron::Client
