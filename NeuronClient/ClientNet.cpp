#include "pch.h"
#include "ClientNet.h"

namespace Neuron::Client
{
  void ClientNet::Startup()
  {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    ASSERT_TEXT(result == 0, "WSAStartup failed: {}\n", result);

    sm_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_TEXT(sm_socket != INVALID_SOCKET, "Socket creation failed\n");

    u_long mode = 1;
    ioctlsocket(sm_socket, FIONBIO, &mode);

    sm_connected = false;
    sm_connecting = false;
    sm_clientId = 0;
    sm_playerObjectId = INVALID_OBJECT_ID;
    sm_sequence = 0;

    DebugTrace("ClientNet started\n");
  }

  void ClientNet::Shutdown()
  {
    if (sm_connected || sm_connecting)
      Disconnect();

    if (sm_socket != INVALID_SOCKET)
    {
      closesocket(sm_socket);
      sm_socket = INVALID_SOCKET;
    }
    sm_snapshotCallback = nullptr;
    WSACleanup();
    DebugTrace("ClientNet shutdown\n");
  }

  void ClientNet::Connect(const char* _serverAddress, uint16_t _port)
  {
    sm_serverAddr = {};
    sm_serverAddr.sin_family = AF_INET;
    sm_serverAddr.sin_port = htons(_port);
    inet_pton(AF_INET, _serverAddress, &sm_serverAddr.sin_addr);

    sm_connecting = true;
    sm_connected = false;

    // Send connect packet
    Net::ConnectPacket pkt;
    pkt.header.type = Net::PacketType::Connect;
    pkt.header.sequence = sm_sequence++;

    sendto(sm_socket, reinterpret_cast<const char*>(&pkt), sizeof(pkt), 0,
           reinterpret_cast<const sockaddr*>(&sm_serverAddr), sizeof(sm_serverAddr));

    DebugTrace("ClientNet connecting to {}:{}\n", _serverAddress, _port);
  }

  void ClientNet::Disconnect()
  {
    if (!sm_connected && !sm_connecting)
      return;

    Net::DisconnectPacket pkt;
    pkt.header.type = Net::PacketType::Disconnect;
    pkt.header.sequence = sm_sequence++;

    sendto(sm_socket, reinterpret_cast<const char*>(&pkt), sizeof(pkt), 0,
           reinterpret_cast<const sockaddr*>(&sm_serverAddr), sizeof(sm_serverAddr));

    sm_connected = false;
    sm_connecting = false;
    sm_clientId = 0;
    sm_playerObjectId = INVALID_OBJECT_ID;
    DebugTrace("ClientNet disconnected\n");
  }

  void ClientNet::Poll()
  {
    uint8_t buffer[4096];
    sockaddr_in fromAddr = {};
    int fromLen = sizeof(fromAddr);

    for (;;)
    {
      int bytes = recvfrom(sm_socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                           reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
      if (bytes <= 0)
        break;

      if (bytes < static_cast<int>(sizeof(Net::PacketHeader)))
        continue;

      auto* header = reinterpret_cast<const Net::PacketHeader*>(buffer);
      if (header->protocolId != Net::PROTOCOL_ID)
        continue;

      switch (header->type)
      {
      case Net::PacketType::ConnectAck:
        if (bytes >= static_cast<int>(sizeof(Net::ConnectAckPacket)))
          HandleConnectAck(*reinterpret_cast<const Net::ConnectAckPacket*>(buffer));
        break;
      case Net::PacketType::WorldSnapshot:
        HandleSnapshot(buffer, bytes);
        break;
      default:
        break;
      }
    }

    // Retry connect if still connecting
    if (sm_connecting && !sm_connected)
    {
      sm_connectTimer += 0.016f; // approximate
      if (sm_connectTimer > 1.0f)
      {
        sm_connectTimer = 0.f;
        Net::ConnectPacket pkt;
        pkt.header.type = Net::PacketType::Connect;
        pkt.header.sequence = sm_sequence++;
        sendto(sm_socket, reinterpret_cast<const char*>(&pkt), sizeof(pkt), 0,
               reinterpret_cast<const sockaddr*>(&sm_serverAddr), sizeof(sm_serverAddr));
      }
    }
  }

  void ClientNet::SendInput(const Net::ClientInputPacket& _input)
  {
    if (!sm_connected)
      return;

    sendto(sm_socket, reinterpret_cast<const char*>(&_input), sizeof(_input), 0,
           reinterpret_cast<const sockaddr*>(&sm_serverAddr), sizeof(sm_serverAddr));
  }

  void ClientNet::SendHeartbeat()
  {
    if (!sm_connected)
      return;

    Net::HeartbeatPacket pkt;
    pkt.header.type = Net::PacketType::Heartbeat;
    pkt.header.sequence = sm_sequence++;
    pkt.clientId = sm_clientId;

    sendto(sm_socket, reinterpret_cast<const char*>(&pkt), sizeof(pkt), 0,
           reinterpret_cast<const sockaddr*>(&sm_serverAddr), sizeof(sm_serverAddr));
  }

  void ClientNet::HandleConnectAck(const Net::ConnectAckPacket& _pkt)
  {
    sm_clientId = _pkt.clientId;
    sm_playerObjectId = _pkt.playerObjectId;
    sm_connected = true;
    sm_connecting = false;
    DebugTrace("ClientNet connected: clientId={} objectId={}\n", sm_clientId, sm_playerObjectId);
  }

  void ClientNet::HandleSnapshot(const uint8_t* _data, int _size)
  {
    // Validate minimum size
    constexpr int headerSize = static_cast<int>(offsetof(Net::WorldSnapshotPacket, objects));
    if (_size < headerSize)
      return;

    const auto* snapshot = reinterpret_cast<const Net::WorldSnapshotPacket*>(_data);

    int expectedSize = headerSize + static_cast<int>(sizeof(Net::SnapshotObjectData)) * snapshot->objectCount;
    if (_size < expectedSize)
      return;

    if (sm_snapshotCallback)
      sm_snapshotCallback(*snapshot);
  }
}
