#include "pch.h"
#include "EntitySystem.h"

namespace Neuron::GameLogic
{

EntityID EntitySystem::spawnEntity(const Entity& prototype)
{
    EntityID id = INVALID_ENTITY;

    // Reuse an ID from the free pool when possible
    if (!m_freePool.empty())
    {
        id = m_freePool.back();
        m_freePool.pop_back();
    }
    else
    {
        id = m_nextId++;
    }

    Entity e  = prototype;
    e.id      = id;
    e.alive   = true;

    // Check if there's an existing dead slot at this index in the lookup
    auto it = m_entityLookup.find(id);
    if (it != m_entityLookup.end())
    {
        // Reuse the slot
        m_entities[it->second] = e;
    }
    else
    {
        // Append to the back
        m_entityLookup[id] = m_entities.size();
        m_entities.push_back(e);
    }

    ++m_liveCount;
    return id;
}

void EntitySystem::destroyEntity(EntityID id)
{
    auto it = m_entityLookup.find(id);
    if (it == m_entityLookup.end())
        return;

    size_t idx = it->second;
    if (!m_entities[idx].alive)
        return;

    m_entities[idx].alive = false;
    m_entities[idx].id    = INVALID_ENTITY;
    m_freePool.push_back(id);

    m_entityLookup.erase(it);
    --m_liveCount;
}

Entity* EntitySystem::getEntity(EntityID id)
{
    auto it = m_entityLookup.find(id);
    if (it == m_entityLookup.end())
        return nullptr;
    Entity& e = m_entities[it->second];
    return e.alive ? &e : nullptr;
}

const Entity* EntitySystem::getEntity(EntityID id) const
{
    auto it = m_entityLookup.find(id);
    if (it == m_entityLookup.end())
        return nullptr;
    const Entity& e = m_entities[it->second];
    return e.alive ? &e : nullptr;
}

void EntitySystem::tickUpdate(float dt, uint64_t tickNum)
{
    for (auto& e : m_entities)
    {
        if (!e.alive) continue;

        e.vel += e.accel * dt;
        e.pos += e.vel * dt;
        e.lastUpdateTick = tickNum;
    }
}

} // namespace Neuron::GameLogic
