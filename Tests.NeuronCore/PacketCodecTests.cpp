#include "pch.h"
#include "PacketCodec.h"
#include "PacketTypes.h"
#include "Constants.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{
  TEST_CLASS(PacketCodecRoundTripTests)
  {
    public:
      TEST_METHOD(RoundTrip_CmdInput)
      {
        CmdInput cmd{};
        cmd.playerId = 42;
        cmd.action = ActionType::Move;
        cmd.targetX = 1.0f;
        cmd.targetY = 2.0f;
        cmd.targetZ = 3.0f;
        cmd.targetEntity = 99;

        auto bytes = encodePacket(cmd, 1);
        Assert::IsTrue(bytes.size() > PACKET_HEADER_SIZE);

        DecodedPacket out;
        auto result = decodePacket(bytes, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::Ok), static_cast<int>(result));
        Assert::AreEqual(CmdInput::TYPE, out.header.type);
        Assert::AreEqual(static_cast<uint16_t>(sizeof(CmdInput)), out.header.payloadSize);
        Assert::AreEqual(1u, out.header.sequence);

        // Verify payload round-trips correctly
        Assert::AreEqual(sizeof(CmdInput), out.payload.size());
        CmdInput decoded;
        std::memcpy(&decoded, out.payload.data(), sizeof(decoded));
        Assert::AreEqual(static_cast<uint16_t>(42), static_cast<uint16_t>(decoded.playerId));
        Assert::AreEqual(static_cast<uint8_t>(ActionType::Move), static_cast<uint8_t>(decoded.action));
        Assert::AreEqual(1.0f, decoded.targetX);
        Assert::AreEqual(2.0f, decoded.targetY);
        Assert::AreEqual(3.0f, decoded.targetZ);
        Assert::AreEqual(99u, decoded.targetEntity);
      }

      TEST_METHOD(RoundTrip_CmdChat)
      {
        CmdChat chat{};
        chat.playerId = 7;
        auto msg = "Hello world";
        strncpy_s(chat.message, msg, sizeof(chat.message) - 1);
        chat.messageLen = static_cast<uint16_t>(std::strlen(msg));

        auto bytes = encodePacket(chat, 10);

        DecodedPacket out;
        auto result = decodePacket(bytes, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::Ok), static_cast<int>(result));
        Assert::AreEqual(CmdChat::TYPE, out.header.type);

        CmdChat decoded;
        std::memcpy(&decoded, out.payload.data(), sizeof(decoded));
        Assert::AreEqual(static_cast<uint16_t>(7), static_cast<uint16_t>(decoded.playerId));
        Assert::AreEqual(0, std::strncmp("Hello world", decoded.message, 11));
      }

      TEST_METHOD(RoundTrip_CmdRequestChunk)
      {
        CmdRequestChunk req{};
        req.playerId = 5;
        req.chunkId = makeChunkID(1, 2, 3, 4, 5);

        auto bytes = encodePacket(req, 20);

        DecodedPacket out;
        auto result = decodePacket(bytes, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::Ok), static_cast<int>(result));
        Assert::AreEqual(CmdRequestChunk::TYPE, out.header.type);

        CmdRequestChunk decoded;
        std::memcpy(&decoded, out.payload.data(), sizeof(decoded));
        Assert::AreEqual(static_cast<uint16_t>(5), static_cast<uint16_t>(decoded.playerId));
        Assert::AreEqual(req.chunkId, decoded.chunkId);
      }

      TEST_METHOD(SnapState_ExceedsMTU)
      {
        // SnapState with MAX_ENTITIES_PER_SNAP (64) entities is larger than
        // MAX_PACKET_SIZE (1400). The codec correctly rejects it as Oversized.
        // In production, snapshots are delta-encoded into smaller packets (Phase 5).
        SnapState snap{};
        snap.serverTick  = 12345;
        snap.entityCount = 2;

        auto bytes = encodePacket(snap, 100);
        Assert::IsTrue(bytes.size() > MAX_PACKET_SIZE);

        DecodedPacket out;
        auto result = decodePacket(bytes, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::Oversized), static_cast<int>(result));
      }

      TEST_METHOD(RoundTrip_PingPacket)
      {
        PingPacket ping{};
        ping.serverTick = 9999;
        ping.serverTimeUs = 1234567890;

        auto bytes = encodePacket(ping, 50);

        DecodedPacket out;
        auto result = decodePacket(bytes, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::Ok), static_cast<int>(result));
        Assert::AreEqual(PingPacket::TYPE, out.header.type);

        PingPacket decoded;
        std::memcpy(&decoded, out.payload.data(), sizeof(decoded));
        Assert::AreEqual(static_cast<uint64_t>(9999), decoded.serverTick);
        Assert::AreEqual(static_cast<uint64_t>(1234567890), decoded.serverTimeUs);
      }

      TEST_METHOD(MagicFieldIsCorrect)
      {
        CmdInput cmd{};
        auto bytes = encodePacket(cmd, 1);

        // First 4 bytes must be PACKET_MAGIC (0x53535452 = "SSTR")
        uint32_t magic = 0;
        std::memcpy(&magic, bytes.data(), 4);
        Assert::AreEqual(PACKET_MAGIC, magic);
      }

      TEST_METHOD(SequenceNumberPreserved)
      {
        CmdInput cmd{};
        auto bytes = encodePacket(cmd, 42);

        DecodedPacket out;
        decodePacket(bytes, out);
        Assert::AreEqual(42u, out.header.sequence);
      }
  };

  TEST_CLASS(PacketCodecErrorTests)
  {
    public:
      TEST_METHOD(CRCDetectsCorruption)
      {
        CmdInput cmd{};
        cmd.playerId = 42;
        auto bytes = encodePacket(cmd, 1);

        // Corrupt last byte of payload
        bytes.back() ^= 0xFF;

        DecodedPacket out;
        auto result = decodePacket(bytes, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::BadCrc), static_cast<int>(result));
      }

      TEST_METHOD(MagicMismatch)
      {
        CmdInput cmd{};
        auto bytes = encodePacket(cmd, 1);

        // Corrupt magic bytes
        bytes[0] = 0x00;
        bytes[1] = 0x00;

        DecodedPacket out;
        auto result = decodePacket(bytes, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::BadMagic), static_cast<int>(result));
      }

      TEST_METHOD(TooShortPacket)
      {
        std::vector<uint8_t> tiny(4, 0);

        DecodedPacket out;
        auto result = decodePacket(tiny, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::TooShort), static_cast<int>(result));
      }

      TEST_METHOD(ExactlyHeaderSizeWithZeroPayload)
      {
        // A packet with exactly the header size and payloadSize=0 should be Ok
        PacketHeader hdr;
        hdr.payloadSize = 0;
        hdr.type = CmdInput::TYPE;
        hdr.sequence = 1;
        hdr.crc = crc32c({});

        std::vector<uint8_t> buf(PACKET_HEADER_SIZE);
        auto* p = buf.data();
        std::memcpy(p, &hdr.magic, 4);
        p += 4;
        std::memcpy(p, &hdr.type, 1);
        p += 1;
        std::memcpy(p, &hdr.flags, 1);
        p += 1;
        std::memcpy(p, &hdr.reserved, 2);
        p += 2;
        std::memcpy(p, &hdr.sequence, 4);
        p += 4;
        std::memcpy(p, &hdr.payloadSize, 2);
        p += 2;
        std::memcpy(p, &hdr.crc, 4);

        DecodedPacket out;
        auto result = decodePacket(buf, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::Ok), static_cast<int>(result));
        Assert::AreEqual(static_cast<uint16_t>(0), out.header.payloadSize);
      }

      TEST_METHOD(EmptyBufferIsTooShort)
      {
        std::vector<uint8_t> empty;

        DecodedPacket out;
        auto result = decodePacket(empty, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::TooShort), static_cast<int>(result));
      }

      TEST_METHOD(SingleByteLessThanHeaderIsTooShort)
      {
        std::vector<uint8_t> almostHeader(PACKET_HEADER_SIZE - 1, 0);

        DecodedPacket out;
        auto result = decodePacket(almostHeader, out);
        Assert::AreEqual(static_cast<int>(DecodeResult::TooShort), static_cast<int>(result));
      }
  };

  TEST_CLASS(CRC32CTests)
  {
    public:
      TEST_METHOD(EmptyDataProducesConsistentCRC)
      {
        std::vector<uint8_t> empty;
        auto crc1 = crc32c(empty);
        auto crc2 = crc32c(empty);
        Assert::AreEqual(crc1, crc2);
      }

      TEST_METHOD(DifferentDataProducesDifferentCRC)
      {
        std::vector<uint8_t> a{1, 2, 3, 4};
        std::vector<uint8_t> b{5, 6, 7, 8};
        Assert::AreNotEqual(crc32c(a), crc32c(b));
      }

      TEST_METHOD(SameDataProducesSameCRC)
      {
        std::vector<uint8_t> data{0x48, 0x65, 0x6C, 0x6C, 0x6F};
        auto crc1 = crc32c(data);
        auto crc2 = crc32c(data);
        Assert::AreEqual(crc1, crc2);
      }

      TEST_METHOD(SingleBitFlipChangesCRC)
      {
        std::vector<uint8_t> original{1, 2, 3, 4, 5, 6, 7, 8};
        auto crcOriginal = crc32c(original);

        std::vector<uint8_t> flipped = original;
        flipped[3] ^= 0x01;
        auto crcFlipped = crc32c(flipped);

        Assert::AreNotEqual(crcOriginal, crcFlipped);
      }
  };
} // namespace Tests
