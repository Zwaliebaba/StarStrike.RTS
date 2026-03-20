#include "pch.h"

#include "Socket.h"
#include "PacketCodec.h"
#include "PacketTypes.h"
#include "SnapshotDecoder.h"

#include <set>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{

TEST_CLASS(ClientServerRoundTripTests)
{
public:
    // Integration test: real UDP sockets on loopback.
    // Server socket binds, client socket sends, server validates.

    TEST_METHOD(ClientSendsCmdInput_ServerReceives)
    {
        Neuron::UDPSocket server;
        Assert::IsTrue(server.bind("127.0.0.1", 0));
        uint16_t serverPort = server.localPort();
        Assert::AreNotEqual(uint16_t(0), serverPort);

        // Encode a CmdInput packet
        Neuron::CmdInput cmd{};
        cmd.playerId = 42;
        cmd.action   = Neuron::ActionType::Move;
        cmd.targetX  = 10.0f;
        cmd.targetY  = 20.0f;
        cmd.targetZ  = 30.0f;
        auto bytes = Neuron::encodePacket(cmd, 1);

        // Send from a separate socket (simulates client)
        Neuron::UDPSocket client;
        Assert::IsTrue(client.bind("127.0.0.1", 0));
        Assert::IsTrue(client.sendTo(bytes, "127.0.0.1", serverPort));

        // Give the OS a moment to deliver
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Server receives
        auto datagram = server.recvFrom();
        Assert::IsTrue(datagram.has_value());

        Neuron::DecodedPacket decoded;
        auto result = Neuron::decodePacket(datagram->data, decoded);
        Assert::AreEqual(static_cast<int>(Neuron::DecodeResult::Ok), static_cast<int>(result));
        Assert::AreEqual(static_cast<uint8_t>(Neuron::PacketType::CmdInput), decoded.header.type);

        // Verify payload
        Assert::AreEqual(static_cast<uint16_t>(sizeof(Neuron::CmdInput)), decoded.header.payloadSize);
        Neuron::CmdInput received;
        std::memcpy(&received, decoded.payload.data(), sizeof(received));
        Assert::AreEqual(Neuron::PlayerID(42), received.playerId);
        Assert::AreEqual(10.0f, received.targetX);
        Assert::AreEqual(20.0f, received.targetY);
        Assert::AreEqual(30.0f, received.targetZ);

        server.close();
        client.close();
    }

    TEST_METHOD(ServerSendsPing_ClientDecodes)
    {
        Neuron::UDPSocket server;
        Assert::IsTrue(server.bind("127.0.0.1", 0));
        uint16_t serverPort = server.localPort();

        Neuron::UDPSocket client;
        Assert::IsTrue(client.bind("127.0.0.1", 0));
        uint16_t clientPort = client.localPort();

        // Build a PingPacket (small enough for the wire codec)
        Neuron::PingPacket ping{};
        ping.serverTick  = 120;
        ping.serverTimeUs = 999999;

        auto bytes = Neuron::encodePacket(ping, 99);

        // Server sends ping to client
        Assert::IsTrue(server.sendTo(bytes, "127.0.0.1", clientPort));

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Client receives
        auto datagram = client.recvFrom();
        Assert::IsTrue(datagram.has_value());

        Neuron::DecodedPacket decoded;
        auto decResult = Neuron::decodePacket(datagram->data, decoded);
        Assert::AreEqual(static_cast<int>(Neuron::DecodeResult::Ok), static_cast<int>(decResult));
        Assert::AreEqual(Neuron::PingPacket::TYPE, decoded.header.type);

        // Verify payload
        Assert::AreEqual(static_cast<uint16_t>(sizeof(Neuron::PingPacket)), decoded.header.payloadSize);
        Neuron::PingPacket received;
        std::memcpy(&received, decoded.payload.data(), sizeof(received));
        Assert::AreEqual(uint64_t(120), received.serverTick);
        Assert::AreEqual(uint64_t(999999), received.serverTimeUs);

        server.close();
        client.close();
    }

    TEST_METHOD(FullRoundTrip_ClientSendsCmd_ServerRespondsPing)
    {
        Neuron::UDPSocket server;
        Assert::IsTrue(server.bind("127.0.0.1", 0));
        uint16_t serverPort = server.localPort();

        Neuron::UDPSocket client;
        Assert::IsTrue(client.bind("127.0.0.1", 0));

        // 1. Client sends CmdInput
        Neuron::CmdInput cmd{};
        cmd.playerId = 1;
        cmd.action   = Neuron::ActionType::Attack;
        auto cmdBytes = Neuron::encodePacket(cmd, 1);
        Assert::IsTrue(client.sendTo(cmdBytes, "127.0.0.1", serverPort));

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 2. Server receives command
        auto cmdDatagram = server.recvFrom();
        Assert::IsTrue(cmdDatagram.has_value());

        Neuron::DecodedPacket decodedCmd;
        auto cmdResult = Neuron::decodePacket(cmdDatagram->data, decodedCmd);
        Assert::AreEqual(static_cast<int>(Neuron::DecodeResult::Ok), static_cast<int>(cmdResult));

        // 3. Server responds with a PingPacket (small enough for wire codec;
        //    SnapState exceeds MAX_PACKET_SIZE and is tested via unit tests)
        Neuron::PingPacket ping{};
        ping.serverTick  = 60;
        ping.serverTimeUs = 123456;

        auto pingBytes = Neuron::encodePacket(ping, 1);

        // Send back to the client's address
        Assert::IsTrue(server.sendTo(pingBytes, cmdDatagram->senderAddr, cmdDatagram->senderPort));

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 4. Client receives response
        auto respDatagram = client.recvFrom();
        Assert::IsTrue(respDatagram.has_value());

        Neuron::DecodedPacket decodedResp;
        auto respResult = Neuron::decodePacket(respDatagram->data, decodedResp);
        Assert::AreEqual(static_cast<int>(Neuron::DecodeResult::Ok), static_cast<int>(respResult));
        Assert::AreEqual(Neuron::PingPacket::TYPE, decodedResp.header.type);

        Neuron::PingPacket received;
        std::memcpy(&received, decodedResp.payload.data(), sizeof(received));
        Assert::AreEqual(uint64_t(60), received.serverTick);
        Assert::AreEqual(uint64_t(123456), received.serverTimeUs);

        server.close();
        client.close();
    }

    TEST_METHOD(NonBlockingRecv_ReturnsNulloptWhenEmpty)
    {
        Neuron::UDPSocket sock;
        Assert::IsTrue(sock.bind("127.0.0.1", 0));

        // No data sent — recvFrom should return nullopt immediately
        auto result = sock.recvFrom();
        Assert::IsFalse(result.has_value());

        sock.close();
    }

    TEST_METHOD(PingPacketRoundTrip)
    {
        Neuron::UDPSocket server;
        Assert::IsTrue(server.bind("127.0.0.1", 0));
        uint16_t serverPort = server.localPort();

        Neuron::UDPSocket client;
        Assert::IsTrue(client.bind("127.0.0.1", 0));

        // Client sends a ping
        Neuron::PingPacket ping;
        ping.serverTick  = 0;
        ping.serverTimeUs = 0;
        auto bytes = Neuron::encodePacket(ping, 0);
        Assert::IsTrue(client.sendTo(bytes, "127.0.0.1", serverPort));

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        auto datagram = server.recvFrom();
        Assert::IsTrue(datagram.has_value());

        Neuron::DecodedPacket decoded;
        auto result = Neuron::decodePacket(datagram->data, decoded);
        Assert::AreEqual(static_cast<int>(Neuron::DecodeResult::Ok), static_cast<int>(result));
        Assert::AreEqual(static_cast<uint8_t>(Neuron::PacketType::Ping), decoded.header.type);

        server.close();
        client.close();
    }
};

TEST_CLASS(PacketTypeCollisionTests)
{
public:
    TEST_METHOD(CmdInputAndSnapStateHaveDistinctTypeBytes)
    {
        // Regression: CmdInput (0x0001) and SnapState (0x0101) used to collide
        // when truncated to uint8_t.  After fix, they must be distinct.
        Assert::AreNotEqual(Neuron::CmdInput::TYPE, Neuron::SnapState::TYPE);
    }

    TEST_METHOD(AllPacketTypesAreUnique)
    {
        std::set<uint8_t> seen;
        seen.insert(Neuron::CmdInput::TYPE);
        seen.insert(Neuron::CmdChat::TYPE);
        seen.insert(Neuron::CmdRequestChunk::TYPE);
        seen.insert(Neuron::SnapState::TYPE);
        seen.insert(Neuron::PingPacket::TYPE);

        // 5 types inserted → set should have 5 entries (no duplicates)
        Assert::AreEqual(size_t(5), seen.size());
    }

    TEST_METHOD(ServerToClientTypesHaveHighBitSet)
    {
        Assert::IsTrue(Neuron::SnapState::TYPE >= 0x80);
        Assert::IsTrue(Neuron::PingPacket::TYPE >= 0x80);
    }

    TEST_METHOD(ClientToServerTypesHaveHighBitClear)
    {
        Assert::IsTrue(Neuron::CmdInput::TYPE < 0x80);
        Assert::IsTrue(Neuron::CmdChat::TYPE < 0x80);
        Assert::IsTrue(Neuron::CmdRequestChunk::TYPE < 0x80);
    }
};

TEST_CLASS(MultiClientStressTests)
{
public:
    TEST_METHOD(TenGhostClients_SendAndReceive)
    {
        // Simulate 10 ghost clients each sending CmdInput to a server socket.
        // Verifies the network path handles concurrent senders.
        static constexpr int NUM_CLIENTS     = 10;
        static constexpr int PACKETS_PER_CLIENT = 30;

        Neuron::UDPSocket server;
        Assert::IsTrue(server.bind("127.0.0.1", 0));
        uint16_t serverPort = server.localPort();

        // Create ghost client sockets
        std::vector<Neuron::UDPSocket> clients(NUM_CLIENTS);
        for (int i = 0; i < NUM_CLIENTS; ++i)
            Assert::IsTrue(clients[i].bind("127.0.0.1", 0));

        // Each client sends PACKETS_PER_CLIENT CmdInput packets
        uint32_t totalSent = 0;
        for (int i = 0; i < NUM_CLIENTS; ++i)
        {
            for (int p = 0; p < PACKETS_PER_CLIENT; ++p)
            {
                Neuron::CmdInput cmd{};
                cmd.playerId = static_cast<Neuron::PlayerID>(i);
                cmd.action   = Neuron::ActionType::Move;
                cmd.targetX  = static_cast<float>(i * 10 + p);
                auto bytes = Neuron::encodePacket(cmd, static_cast<uint32_t>(i * 1000 + p));
                Assert::IsTrue(clients[i].sendTo(bytes, "127.0.0.1", serverPort));
                ++totalSent;
            }
        }

        // Give the OS time to deliver all datagrams
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Server drains all packets and validates each one
        uint32_t received = 0;
        uint32_t valid    = 0;
        for (uint32_t attempt = 0; attempt < totalSent + 50; ++attempt)
        {
            auto datagram = server.recvFrom();
            if (!datagram.has_value())
                break;

            ++received;
            Neuron::DecodedPacket decoded;
            auto result = Neuron::decodePacket(datagram->data, decoded);
            if (result == Neuron::DecodeResult::Ok &&
                decoded.header.type == Neuron::CmdInput::TYPE)
            {
                ++valid;
            }
        }

        // Loopback should deliver all packets (no loss on localhost)
        Assert::AreEqual(totalSent, received,
            L"Not all packets received from 10 ghost clients");
        Assert::AreEqual(totalSent, valid,
            L"Some packets failed decode validation");

        // Clean up
        for (auto& c : clients) c.close();
        server.close();
    }
};

} // namespace Tests
