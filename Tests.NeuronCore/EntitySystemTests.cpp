#include "pch.h"

#include "EntitySystem.h"
#include "Types.h"
#include "Constants.h"

#include <set>
#include <random>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;
using namespace Neuron::GameLogic;

namespace Tests
{

TEST_CLASS(EntitySystemTests)
{
public:
    TEST_METHOD(SpawnReturnsValidID)
    {
        EntitySystem sys;
        Entity e;
        e.type = EntityType::Ship;
        e.hp   = 100;
        auto id = sys.spawnEntity(e);
        Assert::AreNotEqual(INVALID_ENTITY, id);
        Assert::AreEqual(static_cast<size_t>(1), sys.liveCount());
    }

    TEST_METHOD(GetEntityReturnsPointer)
    {
        EntitySystem sys;
        Entity e;
        e.type = EntityType::Asteroid;
        e.hp   = 500;
        auto id = sys.spawnEntity(e);

        auto* ptr = sys.getEntity(id);
        Assert::IsNotNull(ptr);
        Assert::AreEqual(id, ptr->id);
        Assert::AreEqual(static_cast<uint8_t>(EntityType::Asteroid),
                         static_cast<uint8_t>(ptr->type));
        Assert::AreEqual(500u, ptr->hp);
    }

    TEST_METHOD(DestroyEntityReturnsNullptr)
    {
        EntitySystem sys;
        Entity e;
        auto id = sys.spawnEntity(e);
        Assert::IsNotNull(sys.getEntity(id));

        sys.destroyEntity(id);
        Assert::IsNull(sys.getEntity(id));
        Assert::AreEqual(static_cast<size_t>(0), sys.liveCount());
    }

    TEST_METHOD(DestroyInvalidIDDoesNotCrash)
    {
        EntitySystem sys;
        sys.destroyEntity(INVALID_ENTITY);
        sys.destroyEntity(9999);
        Assert::AreEqual(static_cast<size_t>(0), sys.liveCount());
    }

    TEST_METHOD(FreePoolReusesIDs)
    {
        EntitySystem sys;
        Entity e;
        auto id0 = sys.spawnEntity(e);
        auto id1 = sys.spawnEntity(e);
        auto id2 = sys.spawnEntity(e);

        // Destroy middle entity
        sys.destroyEntity(id1);
        Assert::AreEqual(static_cast<size_t>(2), sys.liveCount());

        // Spawn should reuse id1
        auto reused = sys.spawnEntity(e);
        Assert::AreEqual(id1, reused);
        Assert::IsNotNull(sys.getEntity(reused));
        Assert::AreEqual(static_cast<size_t>(3), sys.liveCount());
    }

    TEST_METHOD(FreePoolCorrectness_100K)
    {
        EntitySystem sys;
        Entity e;
        e.type = EntityType::Ship;

        constexpr size_t COUNT = 100'000;
        std::vector<EntityID> ids;
        ids.reserve(COUNT);

        // Spawn 100K entities
        for (size_t i = 0; i < COUNT; ++i)
        {
            ids.push_back(sys.spawnEntity(e));
        }
        Assert::AreEqual(COUNT, sys.liveCount());

        // Verify no duplicate IDs
        std::set<EntityID> uniqueIds(ids.begin(), ids.end());
        Assert::AreEqual(COUNT, uniqueIds.size());

        // Destroy random half
        std::mt19937 rng(42);
        std::vector<EntityID> toDestroy;
        for (size_t i = 0; i < COUNT; ++i)
        {
            if (rng() % 2 == 0)
                toDestroy.push_back(ids[i]);
        }

        for (auto id : toDestroy)
            sys.destroyEntity(id);

        size_t expectedLive = COUNT - toDestroy.size();
        Assert::AreEqual(expectedLive, sys.liveCount());

        // Re-spawn to fill free pool — should reuse IDs, no collisions
        std::set<EntityID> newIds;
        for (size_t i = 0; i < toDestroy.size(); ++i)
        {
            auto newId = sys.spawnEntity(e);
            Assert::AreNotEqual(INVALID_ENTITY, newId);
            newIds.insert(newId);
        }

        // No duplicate IDs among newly spawned
        Assert::AreEqual(toDestroy.size(), newIds.size());
        Assert::AreEqual(COUNT, sys.liveCount());
    }

    TEST_METHOD(TickUpdateMovesEntities)
    {
        EntitySystem sys;
        Entity e;
        e.pos = { 0.0f, 0.0f, 0.0f };
        e.vel = { 1.0f, 2.0f, 3.0f };
        e.accel = { 0.0f, 0.0f, 0.0f };
        auto id = sys.spawnEntity(e);

        sys.tickUpdate(1.0f, 1);

        auto* ptr = sys.getEntity(id);
        Assert::IsNotNull(ptr);
        Assert::AreEqual(1.0f, ptr->pos.x, 0.001f);
        Assert::AreEqual(2.0f, ptr->pos.y, 0.001f);
        Assert::AreEqual(3.0f, ptr->pos.z, 0.001f);
        Assert::AreEqual(static_cast<uint64_t>(1), ptr->lastUpdateTick);
    }

    TEST_METHOD(TickUpdateAppliesAcceleration)
    {
        EntitySystem sys;
        Entity e;
        e.pos   = { 0.0f, 0.0f, 0.0f };
        e.vel   = { 0.0f, 0.0f, 0.0f };
        e.accel = { 10.0f, 0.0f, 0.0f };
        auto id = sys.spawnEntity(e);

        float dt = 0.5f;
        sys.tickUpdate(dt, 1);

        auto* ptr = sys.getEntity(id);
        Assert::IsNotNull(ptr);
        // vel = accel * dt = 5.0
        Assert::AreEqual(5.0f, ptr->vel.x, 0.001f);
        // pos = vel * dt = 5.0 * 0.5 = 2.5
        Assert::AreEqual(2.5f, ptr->pos.x, 0.001f);
    }

    TEST_METHOD(DeadEntitiesNotUpdated)
    {
        EntitySystem sys;
        Entity e;
        e.pos = { 0.0f, 0.0f, 0.0f };
        e.vel = { 100.0f, 0.0f, 0.0f };
        auto id = sys.spawnEntity(e);

        sys.destroyEntity(id);
        sys.tickUpdate(1.0f, 1);

        // Entity was destroyed — getEntity returns nullptr
        Assert::IsNull(sys.getEntity(id));
    }
};

} // namespace Tests
