#include "pch.h"

#include "SnapshotDecoder.h"
#include "PacketCodec.h"
#include "PacketTypes.h"

#include <cstring>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{

TEST_CLASS(SnapshotDecoderTests)
{
public:
    TEST_METHOD(DecodeValidSnapState)
    {
        Neuron::SnapState snap{};
        snap.serverTick  = 42;
        snap.entityCount = 1;
        snap.entities[0].entityId = 7;
        snap.entities[0].type     = Neuron::EntityType::Ship;
        snap.entities[0].position = { 1.0f, 2.0f, 3.0f };
        snap.entities[0].velocity = { 0.5f, 0.0f, 0.0f };
        snap.entities[0].health   = 100.0f;
        snap.entities[0].ownerId  = 10;

        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&snap), sizeof(snap));

        auto result = Neuron::Client::decodeSnapshot(Neuron::SnapState::TYPE, payload);

        Assert::IsTrue(result.has_value());
        Assert::AreEqual(uint64_t(42), result->serverTick);
        Assert::AreEqual(size_t(1), result->entities.size());
        Assert::AreEqual(Neuron::EntityID(7), result->entities[0].entityId);
        Assert::AreEqual(100.0f, result->entities[0].health);
        Assert::AreEqual(1.0f, result->entities[0].position.x);
        Assert::AreEqual(0.5f, result->entities[0].velocity.x);
        Assert::AreEqual(Neuron::PlayerID(10), result->entities[0].ownerId);
    }

    TEST_METHOD(DecodeWrongType_ReturnsNullopt)
    {
        Neuron::SnapState snap{};
        snap.serverTick  = 1;
        snap.entityCount = 0;

        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&snap), sizeof(snap));

        // Use a type byte (0xFF) that is definitively not SnapState::TYPE.
        // Note: CmdInput::TYPE and SnapState::TYPE collide at 0x01 due to
        // PacketType being uint32_t but TYPE fields truncating to uint8_t.
        auto result = Neuron::Client::decodeSnapshot(0xFF, payload);
        Assert::IsFalse(result.has_value());
    }

    TEST_METHOD(DecodeTooSmallPayload_ReturnsNullopt)
    {
        uint8_t tiny[4] = { 0, 0, 0, 0 };
        auto result = Neuron::Client::decodeSnapshot(
            Neuron::SnapState::TYPE,
            std::span<const uint8_t>(tiny, 4));
        Assert::IsFalse(result.has_value());
    }

    TEST_METHOD(DecodeEntityCountOverflow_ReturnsNullopt)
    {
        Neuron::SnapState snap{};
        snap.serverTick  = 1;
        snap.entityCount = Neuron::SnapState::MAX_ENTITIES_PER_SNAP + 1;

        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&snap), sizeof(snap));

        auto result = Neuron::Client::decodeSnapshot(Neuron::SnapState::TYPE, payload);
        Assert::IsFalse(result.has_value());
    }

    TEST_METHOD(DecodeEmptySnapshot_Valid)
    {
        Neuron::SnapState snap{};
        snap.serverTick  = 99;
        snap.entityCount = 0;

        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&snap), sizeof(snap));

        auto result = Neuron::Client::decodeSnapshot(Neuron::SnapState::TYPE, payload);

        Assert::IsTrue(result.has_value());
        Assert::AreEqual(uint64_t(99), result->serverTick);
        Assert::AreEqual(size_t(0), result->entities.size());
    }

    TEST_METHOD(DecodeMaxEntities)
    {
        Neuron::SnapState snap{};
        snap.serverTick  = 200;
        snap.entityCount = Neuron::SnapState::MAX_ENTITIES_PER_SNAP;

        for (uint32_t i = 0; i < Neuron::SnapState::MAX_ENTITIES_PER_SNAP; ++i)
        {
            snap.entities[i].entityId = i + 1;
            snap.entities[i].position = {
                static_cast<float>(i),
                static_cast<float>(i * 2),
                static_cast<float>(i * 3)
            };
            snap.entities[i].health = static_cast<float>(100 + i);
        }

        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&snap), sizeof(snap));

        auto result = Neuron::Client::decodeSnapshot(Neuron::SnapState::TYPE, payload);

        Assert::IsTrue(result.has_value());
        Assert::AreEqual(size_t(Neuron::SnapState::MAX_ENTITIES_PER_SNAP), result->entities.size());

        // Verify first and last entity
        Assert::AreEqual(Neuron::EntityID(1), result->entities[0].entityId);
        Assert::AreEqual(100.0f, result->entities[0].health);
        Assert::AreEqual(Neuron::EntityID(64), result->entities[63].entityId);
        Assert::AreEqual(163.0f, result->entities[63].health);
    }

    TEST_METHOD(EncodeDecodeRoundTrip)
    {
        // SnapState exceeds MAX_PACKET_SIZE so we test the decode path
        // directly with raw bytes (wire codec round-trip is N/A for this
        // struct — see existing SnapState_ExceedsMTU test in PacketCodecTests).
        Neuron::SnapState snap{};
        snap.serverTick  = 777;
        snap.entityCount = 2;
        snap.entities[0].entityId = 10;
        snap.entities[0].position = { 1.0f, 2.0f, 3.0f };
        snap.entities[0].health   = 50.0f;
        snap.entities[1].entityId = 20;
        snap.entities[1].position = { 4.0f, 5.0f, 6.0f };
        snap.entities[1].health   = 75.0f;

        auto payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&snap), sizeof(snap));

        auto snapResult = Neuron::Client::decodeSnapshot(Neuron::SnapState::TYPE, payload);

        Assert::IsTrue(snapResult.has_value());
        Assert::AreEqual(uint64_t(777), snapResult->serverTick);
        Assert::AreEqual(size_t(2), snapResult->entities.size());
        Assert::AreEqual(Neuron::EntityID(10), snapResult->entities[0].entityId);
        Assert::AreEqual(50.0f, snapResult->entities[0].health);
        Assert::AreEqual(Neuron::EntityID(20), snapResult->entities[1].entityId);
        Assert::AreEqual(75.0f, snapResult->entities[1].health);
    }
};

} // namespace Tests
