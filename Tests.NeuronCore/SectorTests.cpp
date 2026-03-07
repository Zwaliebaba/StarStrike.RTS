#include "pch.h"

#include "Sector.h"
#include "Types.h"
#include "Constants.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;
using namespace Neuron::GameLogic;

namespace Tests
{

TEST_CLASS(SectorTests)
{
public:
    TEST_METHOD(SectorBoundsAtOrigin)
    {
        Sector s(0, 0);
        auto lo = s.minBound();
        auto hi = s.maxBound();

        Assert::AreEqual(0, lo.x);
        Assert::AreEqual(0, lo.y);
        Assert::AreEqual(0, lo.z);
        Assert::AreEqual(SECTOR_SIZE_X, hi.x);
        Assert::AreEqual(SECTOR_SIZE_Y, hi.y);
        Assert::AreEqual(SECTOR_SIZE_Z, hi.z);
    }

    TEST_METHOD(SectorBoundsOffset)
    {
        Sector s(2, 3);
        auto lo = s.minBound();
        Assert::AreEqual(2 * SECTOR_SIZE_X, lo.x);
        Assert::AreEqual(3 * SECTOR_SIZE_Y, lo.y);
    }

    TEST_METHOD(IsInBoundsInterior)
    {
        Sector s(0, 0);
        Assert::IsTrue(s.isInBounds({ 0, 0, 0 }));
        Assert::IsTrue(s.isInBounds({ 256, 256, 128 }));
        Assert::IsTrue(s.isInBounds({ SECTOR_SIZE_X - 1, SECTOR_SIZE_Y - 1, SECTOR_SIZE_Z - 1 }));
    }

    TEST_METHOD(IsInBoundsExclusive)
    {
        Sector s(0, 0);
        Assert::IsFalse(s.isInBounds({ SECTOR_SIZE_X, 0, 0 }));
        Assert::IsFalse(s.isInBounds({ 0, SECTOR_SIZE_Y, 0 }));
        Assert::IsFalse(s.isInBounds({ 0, 0, SECTOR_SIZE_Z }));
        Assert::IsFalse(s.isInBounds({ -1, 0, 0 }));
    }

    TEST_METHOD(WorldPosToChunkIDOrigin)
    {
        Sector s(0, 0);
        auto id = s.worldPosToChunkID({ 0, 0, 0 });
        Assert::AreEqual(makeChunkID(0, 0, 0, 0, 0), id);
    }

    TEST_METHOD(WorldPosToChunkIDOffset)
    {
        Sector s(0, 0);
        // Position (32, 64, 96) → chunk (1, 2, 3) in sector (0,0)
        auto id = s.worldPosToChunkID({ 32, 64, 96 });
        Assert::AreEqual(makeChunkID(0, 0, 1, 2, 3), id);
    }
};

TEST_CLASS(SectorManagerTests_Phase3)
{
public:
    TEST_METHOD(InitCreatesCorrectCount)
    {
        SectorManager mgr;
        mgr.init(SECTOR_GRID_X, SECTOR_GRID_Y);
        Assert::AreEqual(static_cast<size_t>(SECTOR_COUNT), mgr.sectorCount());
    }

    TEST_METHOD(GetSectorReturnsCorrectGrid)
    {
        SectorManager mgr;
        mgr.init(4, 4);

        const auto& s = mgr.getSector(2, 3);
        Assert::AreEqual(2, s.gridX());
        Assert::AreEqual(3, s.gridY());
    }

    TEST_METHOD(FindSectorForWorldPos)
    {
        SectorManager mgr;
        mgr.init(4, 4);

        auto* s = mgr.findSectorForWorldPos({ 0, 0, 0 });
        Assert::IsNotNull(s);
        Assert::AreEqual(0, s->gridX());
        Assert::AreEqual(0, s->gridY());

        // Position in sector (1, 1)
        auto* s2 = mgr.findSectorForWorldPos({
            SECTOR_SIZE_X + 10, SECTOR_SIZE_Y + 10, 0 });
        Assert::IsNotNull(s2);
        Assert::AreEqual(1, s2->gridX());
        Assert::AreEqual(1, s2->gridY());
    }

    TEST_METHOD(FindSectorForWorldPosOutOfBounds)
    {
        SectorManager mgr;
        mgr.init(4, 4);

        auto* s = mgr.findSectorForWorldPos({ -1, -1, 0 });
        Assert::IsNull(s);

        auto* s2 = mgr.findSectorForWorldPos({
            SECTOR_GRID_X * SECTOR_SIZE_X + 1, 0, 0 });
        Assert::IsNull(s2);
    }
};

} // namespace Tests
