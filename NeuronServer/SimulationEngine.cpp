#include "pch.h"
#include "SimulationEngine.h"
#include "ServerLog.h"

#include "Constants.h"
#include "EntitySystem.h"
#include "UniverseManager.h"

#include <cmath>

namespace Neuron::Server
{

void SimulationEngine::init(Neuron::GameLogic::UniverseManager* universe)
{
    m_universe = universe;
    LogInfo("SimulationEngine initialized\n");
}

void SimulationEngine::enqueueCommand(const CmdInput& cmd)
{
    m_pendingCommands.push_back(cmd);
}

void SimulationEngine::tick(uint64_t tickNum)
{
    m_tickCount = tickNum;

    phase1_inputProcessing();
    phase2_voxelUpdate();
    phase3_physics();
    phase4_combat();
    phase5_mining();
    phase6_effects();
}

// ── Phase 1: Process enqueued player commands ───────────────────────────────

void SimulationEngine::phase1_inputProcessing()
{
    if (!m_universe || m_pendingCommands.empty())
        return;

    auto& entitySys = m_universe->getEntitySystem();

    for (const auto& cmd : m_pendingCommands)
    {
        // Find the first alive ship owned by this player.
        // TODO Phase 6: proper player→entity mapping via PlayerStore.
        GameLogic::Entity* playerShip = nullptr;
        for (auto& e : entitySys.getAll())
        {
            if (e.alive && e.type == EntityType::Ship &&
                e.ownerPlayerId == cmd.playerId)
            {
                playerShip = &e;
                break;
            }
        }

        if (!playerShip)
            continue;

        switch (cmd.action)
        {
        case ActionType::Move:
        {
            Vec3 target{ cmd.targetX, cmd.targetY, cmd.targetZ };
            Vec3 delta = target - playerShip->pos;
            float dist = std::sqrt(delta.x * delta.x +
                                   delta.y * delta.y +
                                   delta.z * delta.z);
            if (dist > SHIP_STOP_DISTANCE)
            {
                float inv = SHIP_MOVE_SPEED / dist;
                playerShip->vel = { delta.x * inv, delta.y * inv, delta.z * inv };
                playerShip->targetId = INVALID_ENTITY;
            }
            else
            {
                playerShip->vel = {};
            }
            break;
        }
        case ActionType::Stop:
            playerShip->vel   = {};
            playerShip->accel = {};
            break;

        case ActionType::Attack:
            // Set target for Phase 5 combat processing.
            playerShip->targetId = cmd.targetEntity;
            break;

        case ActionType::Mine:
            // Set target for Phase 5 mining processing.
            playerShip->targetId = cmd.targetEntity;
            break;

        default:
            break;
        }
    }

    m_pendingCommands.clear();
}

void SimulationEngine::phase2_voxelUpdate()
{
    // TODO Phase 5: apply queued voxel modifications
}

void SimulationEngine::phase3_physics()
{
    // Physics integration (pos += vel * dt) is handled by
    // UniverseManager::tick → EntitySystem::tickUpdate,
    // called separately in the server main loop.
    //
    // This phase is reserved for collision detection & response (Phase 5+).
}

void SimulationEngine::phase4_combat()
{
    // TODO Phase 5: weapon fire, projectile spawning, damage, destruction
}

void SimulationEngine::phase5_mining()
{
    // TODO Phase 5: resource extraction logic
}

void SimulationEngine::phase6_effects()
{
    // TODO Phase 5: particle / explosion / visual effect bookkeeping
}

} // namespace Neuron::Server
