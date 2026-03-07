#include "pch.h"

#include "VoxelSystem.h"
#include "Types.h"
#include "Constants.h"

#include <cstring>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Neuron;
using namespace Neuron::GameLogic;

namespace Tests
{

TEST_CLASS(RLECodecTests)
{
public:
    TEST_METHOD(RoundTripEmptyChunk)
    {
        VoxelChunk chunk;
        chunk.clear();

        auto bytes = VoxelSystem::serializeChunk(chunk);
        Assert::IsTrue(bytes.size() > 0);

        // Should be very compact: 1 run of 32768 Empty voxels = 3 bytes
        Assert::AreEqual(static_cast<size_t>(3), bytes.size());

        VoxelChunk decoded;
        bool ok = VoxelSystem::deserializeChunk(bytes, decoded);
        Assert::IsTrue(ok);

        // Byte-by-byte compare
        Assert::AreEqual(0, std::memcmp(chunk.voxels, decoded.voxels, sizeof(chunk.voxels)));
    }

    TEST_METHOD(RoundTripSparseChunk)
    {
        VoxelChunk chunk;
        chunk.clear();

        // Place a single voxel
        chunk.voxels[16][16][16] = static_cast<uint8_t>(VoxelType::Rock);

        auto bytes = VoxelSystem::serializeChunk(chunk);
        Assert::IsTrue(bytes.size() > 0);

        VoxelChunk decoded;
        decoded.clear();
        bool ok = VoxelSystem::deserializeChunk(bytes, decoded);
        Assert::IsTrue(ok);

        Assert::AreEqual(0, std::memcmp(chunk.voxels, decoded.voxels, sizeof(chunk.voxels)));
    }

    TEST_METHOD(RoundTripDenseChunk)
    {
        VoxelChunk chunk;
        // Fill all voxels with Rock
        std::memset(chunk.voxels, static_cast<uint8_t>(VoxelType::Rock), sizeof(chunk.voxels));

        auto bytes = VoxelSystem::serializeChunk(chunk);
        Assert::IsTrue(bytes.size() > 0);

        // Should be compact: 1 run of 32768 Rock voxels = 3 bytes
        Assert::AreEqual(static_cast<size_t>(3), bytes.size());

        VoxelChunk decoded;
        decoded.clear();
        bool ok = VoxelSystem::deserializeChunk(bytes, decoded);
        Assert::IsTrue(ok);

        Assert::AreEqual(0, std::memcmp(chunk.voxels, decoded.voxels, sizeof(chunk.voxels)));
    }

    TEST_METHOD(RoundTripHollowSphere)
    {
        VoxelChunk chunk;
        chunk.clear();

        // Create a hollow sphere (shell of solid voxels around empty interior)
        const int cx = 16, cy = 16, cz = 16;
        const float outerR = 12.0f;
        const float innerR = 10.0f;

        for (int x = 0; x < CHUNK_SIZE; ++x)
        {
            for (int y = 0; y < CHUNK_SIZE; ++y)
            {
                for (int z = 0; z < CHUNK_SIZE; ++z)
                {
                    float dx = static_cast<float>(x - cx);
                    float dy = static_cast<float>(y - cy);
                    float dz = static_cast<float>(z - cz);
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dist <= outerR && dist >= innerR)
                        chunk.voxels[x][y][z] = static_cast<uint8_t>(VoxelType::Crystal);
                }
            }
        }

        auto bytes = VoxelSystem::serializeChunk(chunk);
        Assert::IsTrue(bytes.size() > 0);

        VoxelChunk decoded;
        decoded.clear();
        bool ok = VoxelSystem::deserializeChunk(bytes, decoded);
        Assert::IsTrue(ok);

        Assert::AreEqual(0, std::memcmp(chunk.voxels, decoded.voxels, sizeof(chunk.voxels)));
    }

    TEST_METHOD(RoundTripAlternatingTypes)
    {
        VoxelChunk chunk;
        chunk.clear();

        // Alternating voxel types (worst case for RLE)
        for (int x = 0; x < CHUNK_SIZE; ++x)
        {
            for (int y = 0; y < CHUNK_SIZE; ++y)
            {
                for (int z = 0; z < CHUNK_SIZE; ++z)
                {
                    chunk.voxels[x][y][z] = static_cast<uint8_t>((x + y + z) % 2 == 0
                        ? VoxelType::Empty : VoxelType::Metal);
                }
            }
        }

        auto bytes = VoxelSystem::serializeChunk(chunk);
        Assert::IsTrue(bytes.size() > 0);

        VoxelChunk decoded;
        decoded.clear();
        bool ok = VoxelSystem::deserializeChunk(bytes, decoded);
        Assert::IsTrue(ok);

        Assert::AreEqual(0, std::memcmp(chunk.voxels, decoded.voxels, sizeof(chunk.voxels)));
    }

    TEST_METHOD(DeserializeMalformedDataReturnsFalse)
    {
        // Too short — not enough data to fill a chunk
        std::vector<uint8_t> tooShort = { 0x01, 0x00, 0x00 }; // 1 voxel only

        VoxelChunk chunk;
        bool ok = VoxelSystem::deserializeChunk(tooShort, chunk);
        Assert::IsFalse(ok);
    }

    TEST_METHOD(DeserializeEmptyDataReturnsFalse)
    {
        std::vector<uint8_t> empty;
        VoxelChunk chunk;
        bool ok = VoxelSystem::deserializeChunk(empty, chunk);
        Assert::IsFalse(ok);
    }
};

TEST_CLASS(VoxelSystemTests)
{
public:
    TEST_METHOD(LoadChunkAndGetVoxel)
    {
        VoxelSystem sys;

        VoxelChunk chunk;
        chunk.chunkId   = makeChunkID(0, 0, 0, 0, 0);
        chunk.minCorner = { 0, 0, 0 };
        chunk.clear();
        chunk.voxels[5][10][15] = static_cast<uint8_t>(VoxelType::Ice);

        sys.loadChunk(std::move(chunk));

        Assert::AreEqual(static_cast<uint8_t>(VoxelType::Ice),
                         sys.getVoxel({ 5, 10, 15 }));
        Assert::AreEqual(static_cast<uint8_t>(VoxelType::Empty),
                         sys.getVoxel({ 0, 0, 0 }));
    }

    TEST_METHOD(SetVoxelMarksDirtyAndRecordsDelta)
    {
        VoxelSystem sys;

        VoxelChunk chunk;
        chunk.chunkId   = makeChunkID(0, 0, 0, 0, 0);
        chunk.minCorner = { 0, 0, 0 };
        chunk.clear();

        sys.loadChunk(std::move(chunk));

        sys.setVoxel({ 1, 2, 3 }, static_cast<uint8_t>(VoxelType::Ore), 42, 100);

        // Verify voxel was set
        Assert::AreEqual(static_cast<uint8_t>(VoxelType::Ore),
                         sys.getVoxel({ 1, 2, 3 }));

        // Verify chunk marked dirty
        auto* ch = sys.getChunk(makeChunkID(0, 0, 0, 0, 0));
        Assert::IsNotNull(ch);
        Assert::IsTrue(ch->dirty);

        // Verify delta recorded
        auto deltas = sys.consumeDeltas();
        Assert::AreEqual(static_cast<size_t>(1), deltas.size());
        Assert::AreEqual(static_cast<uint8_t>(VoxelType::Empty), deltas[0].oldType);
        Assert::AreEqual(static_cast<uint8_t>(VoxelType::Ore),   deltas[0].newType);
        Assert::AreEqual(static_cast<uint16_t>(42), static_cast<uint16_t>(deltas[0].playerId));
    }

    TEST_METHOD(ConsumeDeltasClearsBuffer)
    {
        VoxelSystem sys;

        VoxelChunk chunk;
        chunk.chunkId   = makeChunkID(0, 0, 0, 0, 0);
        chunk.minCorner = { 0, 0, 0 };
        chunk.clear();
        sys.loadChunk(std::move(chunk));

        sys.setVoxel({ 0, 0, 0 }, 1, 0, 1);
        auto d1 = sys.consumeDeltas();
        Assert::AreEqual(static_cast<size_t>(1), d1.size());

        auto d2 = sys.consumeDeltas();
        Assert::AreEqual(static_cast<size_t>(0), d2.size());
    }

    TEST_METHOD(GetVoxelOutsideChunkReturnsEmpty)
    {
        VoxelSystem sys;

        VoxelChunk chunk;
        chunk.chunkId   = makeChunkID(0, 0, 0, 0, 0);
        chunk.minCorner = { 0, 0, 0 };
        chunk.clear();
        sys.loadChunk(std::move(chunk));

        // Position outside the loaded chunk
        Assert::AreEqual(static_cast<uint8_t>(VoxelType::Empty),
                         sys.getVoxel({ 100, 100, 100 }));
    }

    TEST_METHOD(ChunkCount)
    {
        VoxelSystem sys;
        Assert::AreEqual(static_cast<size_t>(0), sys.chunkCount());

        VoxelChunk c1;
        c1.chunkId = makeChunkID(0, 0, 0, 0, 0);
        c1.clear();
        sys.loadChunk(std::move(c1));
        Assert::AreEqual(static_cast<size_t>(1), sys.chunkCount());

        VoxelChunk c2;
        c2.chunkId = makeChunkID(0, 0, 1, 0, 0);
        c2.clear();
        sys.loadChunk(std::move(c2));
        Assert::AreEqual(static_cast<size_t>(2), sys.chunkCount());
    }
};

} // namespace Tests
