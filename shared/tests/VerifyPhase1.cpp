#include "pch.h"

#include "NeuronCore/Types.h"
#include "NeuronCore/Constants.h"
#include "Neuron/Socket.h"
#include "Neuron/FileSystem.h"
#include "Neuron/Threading.h"
#include "Neuron/Timer.h"

// Phase 1 verification: ensure all headers compile and types are usable.
static_assert(Neuron::TICK_RATE_HZ == 60);
static_assert(Neuron::CHUNK_SIZE == 32);
static_assert(Neuron::CHUNK_VOLUME == 32 * 32 * 32);
static_assert(Neuron::SECTOR_COUNT == 16);
static_assert(Neuron::MAX_PLAYERS == 50);
static_assert(Neuron::DEFAULT_SERVER_PORT == 7777);
static_assert(Neuron::INVALID_ENTITY == 0xFFFFFFFF);

// Verify ChunkID encoding
static_assert(Neuron::makeChunkID(1, 2, 3, 4, 5)
    == (uint64_t{1} << 40 | uint64_t{2} << 24 | uint64_t{3} << 16
        | uint64_t{4} << 8 | uint64_t{5}));

// Verify enum underlying types
static_assert(sizeof(Neuron::EntityType) == 1);
static_assert(sizeof(Neuron::VoxelType)  == 1);
static_assert(sizeof(Neuron::ActionType) == 1);
static_assert(sizeof(Neuron::ShipType)   == 1);
static_assert(sizeof(Neuron::PacketType) == 4);

// Verify math primitives have expected size
static_assert(sizeof(Neuron::Vec3)  == 12);
static_assert(sizeof(Neuron::Vec3i) == 12);
static_assert(sizeof(Neuron::Quat)  == 16);

// Verify Vec3 operations
constexpr Neuron::Vec3 a{ 1.0f, 2.0f, 3.0f };
constexpr Neuron::Vec3 b{ 4.0f, 5.0f, 6.0f };
constexpr auto sum = a + b;
static_assert(sum.x == 5.0f && sum.y == 7.0f && sum.z == 9.0f);

// Verify AABB containment
constexpr Neuron::AABB box{ { 0.0f, 0.0f, 0.0f }, { 10.0f, 10.0f, 10.0f } };
static_assert(box.contains({ 5.0f, 5.0f, 5.0f }));
static_assert(!box.contains({ 11.0f, 5.0f, 5.0f }));

int main()
{
    // Timer smoke test
    Neuron::Timer timer;
    [[maybe_unused]] auto dt = timer.tick();

    return 0;
}
