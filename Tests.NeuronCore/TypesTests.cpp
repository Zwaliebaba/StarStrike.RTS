#include "pch.h"

#include "Types.h"
#include "Constants.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{

TEST_CLASS(ConstantsTests)
{
public:
    TEST_METHOD(TickRateIs60Hz)
    {
        Assert::AreEqual(60u, Neuron::TICK_RATE_HZ);
    }

    TEST_METHOD(TickIntervalMatchesRate)
    {
        float expected = 1.0f / 60.0f;
        Assert::AreEqual(expected, Neuron::TICK_INTERVAL_SEC);
    }

    TEST_METHOD(ChunkSizeIs32)
    {
        Assert::AreEqual(32, Neuron::CHUNK_SIZE);
    }

    TEST_METHOD(ChunkVolumeIs32Cubed)
    {
        Assert::AreEqual(32 * 32 * 32, Neuron::CHUNK_VOLUME);
    }

    TEST_METHOD(SectorCountIs16)
    {
        Assert::AreEqual(16, Neuron::SECTOR_COUNT);
    }

    TEST_METHOD(MaxPlayersIs50)
    {
        Assert::AreEqual(50u, Neuron::MAX_PLAYERS);
    }

    TEST_METHOD(MaxPacketSizeIs1400)
    {
        Assert::AreEqual(1400u, Neuron::MAX_PACKET_SIZE);
    }

    TEST_METHOD(DefaultServerPortIs7777)
    {
        Assert::AreEqual(static_cast<uint16_t>(7777), Neuron::DEFAULT_SERVER_PORT);
    }

    TEST_METHOD(TicksPerSnapshotIs3)
    {
        Assert::AreEqual(3u, Neuron::TICKS_PER_SNAPSHOT);
    }
};

TEST_CLASS(ChunkIDTests)
{
public:
    TEST_METHOD(MakeChunkIDProducesValidID)
    {
        auto id = Neuron::makeChunkID(1, 2, 3, 4, 5);
        Assert::AreNotEqual(Neuron::INVALID_CHUNK, id);
    }

    TEST_METHOD(MakeChunkIDZeroIsValid)
    {
        auto id = Neuron::makeChunkID(0, 0, 0, 0, 0);
        Assert::AreEqual(static_cast<Neuron::ChunkID>(0), id);
        Assert::AreNotEqual(Neuron::INVALID_CHUNK, id);
    }

    TEST_METHOD(MakeChunkIDEncodesCorrectly)
    {
        auto id = Neuron::makeChunkID(1, 0, 0, 0, 0);
        Assert::AreEqual(static_cast<uint64_t>(1) << 40, id);

        id = Neuron::makeChunkID(0, 1, 0, 0, 0);
        Assert::AreEqual(static_cast<uint64_t>(1) << 24, id);

        id = Neuron::makeChunkID(0, 0, 1, 0, 0);
        Assert::AreEqual(static_cast<uint64_t>(1) << 16, id);

        id = Neuron::makeChunkID(0, 0, 0, 1, 0);
        Assert::AreEqual(static_cast<uint64_t>(1) << 8, id);

        id = Neuron::makeChunkID(0, 0, 0, 0, 1);
        Assert::AreEqual(static_cast<uint64_t>(1), id);
    }

    TEST_METHOD(DifferentInputsProduceDifferentIDs)
    {
        auto id1 = Neuron::makeChunkID(1, 2, 3, 4, 5);
        auto id2 = Neuron::makeChunkID(5, 4, 3, 2, 1);
        Assert::AreNotEqual(id1, id2);
    }
};

TEST_CLASS(Vec3Tests)
{
public:
    TEST_METHOD(DefaultConstructedIsZero)
    {
        Neuron::Vec3 v;
        Assert::AreEqual(0.0f, v.x);
        Assert::AreEqual(0.0f, v.y);
        Assert::AreEqual(0.0f, v.z);
    }

    TEST_METHOD(Addition)
    {
        Neuron::Vec3 a{ 1.0f, 2.0f, 3.0f };
        Neuron::Vec3 b{ 4.0f, 5.0f, 6.0f };
        auto result = a + b;
        Assert::AreEqual(5.0f, result.x);
        Assert::AreEqual(7.0f, result.y);
        Assert::AreEqual(9.0f, result.z);
    }

    TEST_METHOD(Subtraction)
    {
        Neuron::Vec3 a{ 10.0f, 20.0f, 30.0f };
        Neuron::Vec3 b{ 1.0f, 2.0f, 3.0f };
        auto result = a - b;
        Assert::AreEqual(9.0f, result.x);
        Assert::AreEqual(18.0f, result.y);
        Assert::AreEqual(27.0f, result.z);
    }

    TEST_METHOD(ScalarMultiplication)
    {
        Neuron::Vec3 v{ 2.0f, 3.0f, 4.0f };
        auto result = v * 2.0f;
        Assert::AreEqual(4.0f, result.x);
        Assert::AreEqual(6.0f, result.y);
        Assert::AreEqual(8.0f, result.z);
    }

    TEST_METHOD(PlusEquals)
    {
        Neuron::Vec3 v{ 1.0f, 2.0f, 3.0f };
        v += { 10.0f, 20.0f, 30.0f };
        Assert::AreEqual(11.0f, v.x);
        Assert::AreEqual(22.0f, v.y);
        Assert::AreEqual(33.0f, v.z);
    }
};

TEST_CLASS(Vec3iTests)
{
public:
    TEST_METHOD(EqualityOperator)
    {
        Neuron::Vec3i a{ 1, 2, 3 };
        Neuron::Vec3i b{ 1, 2, 3 };
        Assert::IsTrue(a == b);
    }

    TEST_METHOD(InequalityOperator)
    {
        Neuron::Vec3i a{ 1, 2, 3 };
        Neuron::Vec3i b{ 4, 5, 6 };
        Assert::IsTrue(a != b);
    }
};

TEST_CLASS(AABBTests)
{
public:
    TEST_METHOD(ContainsPointInside)
    {
        Neuron::AABB box{ {0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f} };
        Neuron::Vec3 point{ 5.0f, 5.0f, 5.0f };
        Assert::IsTrue(box.contains(point));
    }

    TEST_METHOD(ContainsPointOnBoundary)
    {
        Neuron::AABB box{ {0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f} };
        Neuron::Vec3 point{ 0.0f, 0.0f, 0.0f };
        Assert::IsTrue(box.contains(point));
    }

    TEST_METHOD(DoesNotContainPointOutside)
    {
        Neuron::AABB box{ {0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f} };
        Neuron::Vec3 point{ 11.0f, 5.0f, 5.0f };
        Assert::IsFalse(box.contains(point));
    }

    TEST_METHOD(OverlappingBoxes)
    {
        Neuron::AABB a{ {0.0f, 0.0f, 0.0f}, {10.0f, 10.0f, 10.0f} };
        Neuron::AABB b{ {5.0f, 5.0f, 5.0f}, {15.0f, 15.0f, 15.0f} };
        Assert::IsTrue(a.overlaps(b));
        Assert::IsTrue(b.overlaps(a));
    }

    TEST_METHOD(NonOverlappingBoxes)
    {
        Neuron::AABB a{ {0.0f, 0.0f, 0.0f}, {5.0f, 5.0f, 5.0f} };
        Neuron::AABB b{ {10.0f, 10.0f, 10.0f}, {15.0f, 15.0f, 15.0f} };
        Assert::IsFalse(a.overlaps(b));
        Assert::IsFalse(b.overlaps(a));
    }

    TEST_METHOD(TouchingBoxesOverlap)
    {
        Neuron::AABB a{ {0.0f, 0.0f, 0.0f}, {5.0f, 5.0f, 5.0f} };
        Neuron::AABB b{ {5.0f, 5.0f, 5.0f}, {10.0f, 10.0f, 10.0f} };
        Assert::IsTrue(a.overlaps(b));
    }
};

TEST_CLASS(EnumTests)
{
public:
    TEST_METHOD(EntityTypeSizeIs1Byte)
    {
        Assert::AreEqual(static_cast<size_t>(1), sizeof(Neuron::EntityType));
    }

    TEST_METHOD(VoxelTypeSizeIs1Byte)
    {
        Assert::AreEqual(static_cast<size_t>(1), sizeof(Neuron::VoxelType));
    }

    TEST_METHOD(ActionTypeSizeIs1Byte)
    {
        Assert::AreEqual(static_cast<size_t>(1), sizeof(Neuron::ActionType));
    }

    TEST_METHOD(VoxelTypeEmptyIsZero)
    {
        Assert::AreEqual(static_cast<uint8_t>(0), static_cast<uint8_t>(Neuron::VoxelType::Empty));
    }

    TEST_METHOD(PacketTypeCmdInputIs0x0001)
    {
        Assert::AreEqual(static_cast<uint32_t>(0x0001),
                         static_cast<uint32_t>(Neuron::PacketType::CmdInput));
    }
};

TEST_CLASS(SentinelTests)
{
public:
    TEST_METHOD(InvalidEntityIsMax)
    {
        Assert::AreEqual(std::numeric_limits<uint32_t>::max(), Neuron::INVALID_ENTITY);
    }

    TEST_METHOD(InvalidPlayerIsMax)
    {
        Assert::AreEqual(std::numeric_limits<uint16_t>::max(), Neuron::INVALID_PLAYER);
    }

    TEST_METHOD(InvalidChunkIsMax)
    {
        Assert::AreEqual(std::numeric_limits<uint64_t>::max(), Neuron::INVALID_CHUNK);
    }
};

} // namespace Tests
