#include "pch.h"

#include "EntityCache.h"
#include "PacketTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{

TEST_CLASS(EntityCacheTests)
{
public:
    TEST_METHOD(InitiallyEmpty)
    {
        Neuron::Client::EntityCache cache;
        Assert::AreEqual(size_t(0), cache.count());
        Assert::AreEqual(uint64_t(0), cache.lastTick());
    }

    TEST_METHOD(NewEntitiesAddedFromSnapshot)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap[2]{};
        snap[0].entityId = 1;
        snap[0].type     = Neuron::EntityType::Ship;
        snap[0].position = { 10.0f, 20.0f, 30.0f };
        snap[0].health   = 100.0f;
        snap[0].ownerId  = 42;

        snap[1].entityId = 2;
        snap[1].type     = Neuron::EntityType::Asteroid;
        snap[1].position = { 50.0f, 60.0f, 70.0f };
        snap[1].health   = 500.0f;

        cache.updateFromSnapshot(100, snap, 2);

        Assert::AreEqual(size_t(2), cache.count());
        Assert::AreEqual(uint64_t(100), cache.lastTick());
    }

    TEST_METHOD(GetEntity_Found)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 7;
        snap.position = { 1.0f, 2.0f, 3.0f };
        snap.health   = 75.0f;
        snap.ownerId  = 5;
        snap.type     = Neuron::EntityType::Ship;

        cache.updateFromSnapshot(10, &snap, 1);

        auto* found = cache.getEntity(7);
        Assert::IsNotNull(found);
        Assert::AreEqual(Neuron::EntityID(7), found->id);
        Assert::AreEqual(75.0f, found->health);
        Assert::AreEqual(Neuron::PlayerID(5), found->ownerId);
    }

    TEST_METHOD(GetEntity_NotFound)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 1;
        cache.updateFromSnapshot(10, &snap, 1);

        auto* found = cache.getEntity(999);
        Assert::IsNull(found);
    }

    TEST_METHOD(UpdateExistingEntity)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 1;
        snap.position = { 0.0f, 0.0f, 0.0f };
        snap.health   = 100.0f;
        cache.updateFromSnapshot(1, &snap, 1);

        // Second snapshot updates the same entity
        snap.position = { 10.0f, 20.0f, 30.0f };
        snap.health   = 80.0f;
        cache.updateFromSnapshot(2, &snap, 1);

        // Should still be 1 entity, not 2
        Assert::AreEqual(size_t(1), cache.count());

        auto* e = cache.getEntity(1);
        Assert::IsNotNull(e);
        Assert::AreEqual(80.0f, e->health);
        Assert::AreEqual(10.0f, e->targetPos.x);
        Assert::AreEqual(20.0f, e->targetPos.y);
        Assert::AreEqual(30.0f, e->targetPos.z);
    }

    TEST_METHOD(UpdatePreservesPrevPos)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 1;
        snap.position = { 5.0f, 10.0f, 15.0f };
        cache.updateFromSnapshot(1, &snap, 1);

        // Interpolate to move pos to targetPos
        cache.interpolate(1.0f);

        // Second snapshot
        snap.position = { 50.0f, 100.0f, 150.0f };
        cache.updateFromSnapshot(2, &snap, 1);

        auto* e = cache.getEntity(1);
        Assert::IsNotNull(e);
        // prevPos should be the old pos (before update)
        Assert::AreEqual(5.0f, e->prevPos.x);
        Assert::AreEqual(10.0f, e->prevPos.y);
        Assert::AreEqual(15.0f, e->prevPos.z);
    }

    TEST_METHOD(Interpolate_Alpha0_StaysAtPrev)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 1;
        snap.position = { 100.0f, 200.0f, 300.0f };
        cache.updateFromSnapshot(1, &snap, 1);

        // After first snapshot: prevPos = targetPos = position = (100,200,300)
        // Update with new position
        snap.position = { 200.0f, 400.0f, 600.0f };
        cache.updateFromSnapshot(2, &snap, 1);

        // Interpolate at alpha=0: should stay at prevPos
        cache.interpolate(0.0f);

        auto* e = cache.getEntity(1);
        Assert::IsNotNull(e);
        Assert::AreEqual(e->prevPos.x, e->pos.x);
        Assert::AreEqual(e->prevPos.y, e->pos.y);
        Assert::AreEqual(e->prevPos.z, e->pos.z);
    }

    TEST_METHOD(Interpolate_Alpha1_MovesToTarget)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 1;
        snap.position = { 0.0f, 0.0f, 0.0f };
        cache.updateFromSnapshot(1, &snap, 1);

        snap.position = { 100.0f, 200.0f, 300.0f };
        cache.updateFromSnapshot(2, &snap, 1);

        cache.interpolate(1.0f);

        auto* e = cache.getEntity(1);
        Assert::IsNotNull(e);
        Assert::AreEqual(100.0f, e->pos.x);
        Assert::AreEqual(200.0f, e->pos.y);
        Assert::AreEqual(300.0f, e->pos.z);
    }

    TEST_METHOD(Interpolate_HalfAlpha_Midpoint)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 1;
        snap.position = { 0.0f, 0.0f, 0.0f };
        cache.updateFromSnapshot(1, &snap, 1);

        snap.position = { 100.0f, 200.0f, 300.0f };
        cache.updateFromSnapshot(2, &snap, 1);

        cache.interpolate(0.5f);

        auto* e = cache.getEntity(1);
        Assert::IsNotNull(e);
        Assert::AreEqual(50.0f, e->pos.x, 0.01f);
        Assert::AreEqual(100.0f, e->pos.y, 0.01f);
        Assert::AreEqual(150.0f, e->pos.z, 0.01f);
    }

    TEST_METHOD(Clear_EmptiesCache)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap{};
        snap.entityId = 1;
        cache.updateFromSnapshot(10, &snap, 1);
        Assert::AreEqual(size_t(1), cache.count());

        cache.clear();
        Assert::AreEqual(size_t(0), cache.count());
        Assert::AreEqual(uint64_t(0), cache.lastTick());
        Assert::IsNull(cache.getEntity(1));
    }

    TEST_METHOD(MultipleEntities_IndependentInterpolation)
    {
        Neuron::Client::EntityCache cache;

        Neuron::SnapEntityData snap[2]{};
        snap[0].entityId = 1;
        snap[0].position = { 0.0f, 0.0f, 0.0f };
        snap[1].entityId = 2;
        snap[1].position = { 100.0f, 100.0f, 100.0f };
        cache.updateFromSnapshot(1, snap, 2);

        snap[0].position = { 10.0f, 20.0f, 30.0f };
        snap[1].position = { 200.0f, 200.0f, 200.0f };
        cache.updateFromSnapshot(2, snap, 2);

        cache.interpolate(1.0f);

        auto* e1 = cache.getEntity(1);
        auto* e2 = cache.getEntity(2);
        Assert::IsNotNull(e1);
        Assert::IsNotNull(e2);
        Assert::AreEqual(10.0f, e1->pos.x);
        Assert::AreEqual(200.0f, e2->pos.x);
    }
};

} // namespace Tests
