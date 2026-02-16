#include "pch.h"
#include "ServerNet.h"

namespace Neuron::Server
{
  void ServerNet::Startup(uint16_t _port)
  {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    ASSERT_TEXT(result == 0, "WSAStartup failed: {}\n", result);

    sm_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_TEXT(sm_socket != INVALID_SOCKET, "Socket creation failed\n");

    // Non-blocking
    u_long mode = 1;
    ioctlsocket(sm_socket, FIONBIO, &mode);

    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(_port);
    bindAddr.sin_addr.s_addr = INADDR_ANY;

    result = bind(sm_socket, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr));
    ASSERT_TEXT(result != SOCKET_ERROR, "Bind failed: {}\n", WSAGetLastError());

    sm_clients.clear();
    sm_nextClientId = 1;
    sm_sequence = 0;

    DebugTrace("ServerNet started on port {}\n", _port);
    std::cout << "[Server] Listening on port " << _port << std::endl;
  }

  void ServerNet::Shutdown()
  {
    if (sm_socket != INVALID_SOCKET)
    {
      closesocket(sm_socket);
      sm_socket = INVALID_SOCKET;
    }
    sm_clients.clear();
    sm_onConnect = nullptr;
    sm_onDisconnect = nullptr;
    sm_onInput = nullptr;
    WSACleanup();
    DebugTrace("ServerNet shutdown\n");
  }

  void ServerNet::Poll()
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
      case Net::PacketType::Connect:
        HandleConnect(fromAddr);
        break;
      case Net::PacketType::Disconnect:
        if (bytes >= static_cast<int>(sizeof(Net::DisconnectPacket)))
          HandleDisconnect(*reinterpret_cast<const Net::DisconnectPacket*>(buffer), fromAddr);
        break;
      case Net::PacketType::ClientInput:
        if (bytes >= static_cast<int>(sizeof(Net::ClientInputPacket)))
          HandleInput(*reinterpret_cast<const Net::ClientInputPacket*>(buffer));
        break;
      case Net::PacketType::Heartbeat:
        if (bytes >= static_cast<int>(sizeof(Net::HeartbeatPacket)))
          HandleHeartbeat(*reinterpret_cast<const Net::HeartbeatPacket*>(buffer));
        break;
      default:
        break;
      }
    }
  }

  void ServerNet::SendToClient(uint32_t _clientId, const void* _data, int _size)
  {
    auto it = sm_clients.find(_clientId);
    if (it == sm_clients.end())
      return;

    sendto(sm_socket, reinterpret_cast<const char*>(_data), _size, 0,
           reinterpret_cast<const sockaddr*>(&it->second.address), sizeof(it->second.address));
  }

  void ServerNet::BroadcastToAll(const void* _data, int _size)
  {
    for (const auto& [id, client] : sm_clients)
    {
      sendto(sm_socket, reinterpret_cast<const char*>(_data), _size, 0,
             reinterpret_cast<const sockaddr*>(&client.address), sizeof(client.address));
    }
  }

  void ServerNet::SetPlayerObjectId(uint32_t _clientId, ObjectId _objectId)
  {
    auto it = sm_clients.find(_clientId);
    if (it != sm_clients.end())
      it->second.playerObjectId = _objectId;
  }

  void ServerNet::HandleConnect(const sockaddr_in& _addr)
  {
    // Check if already connected
    uint32_t existingId = FindClientByAddress(_addr);
    if (existingId != 0)
    {
      // Resend ConnectAck
      auto& client = sm_clients[existingId];
      Net::ConnectAckPacket ack;
      ack.header.type = Net::PacketType::ConnectAck;
      ack.header.sequence = sm_sequence++;
      ack.clientId = client.clientId;
      ack.playerObjectId = client.playerObjectId;
      SendToClient(existingId, &ack, sizeof(ack));
      return;
    }

    uint32_t newId = sm_nextClientId++;
    ClientConnection conn;
    conn.address = _addr;
    conn.clientId = newId;
    conn.heartbeatTimer = 0.f;
    sm_clients[newId] = conn;

    std::cout << "[Server] Client " << newId << " connected" << std::endl;

    if (sm_onConnect)
      sm_onConnect(newId, _addr);

    // Send ConnectAck (playerObjectId set by handler)
    auto& updatedClient = sm_clients[newId];
    Net::ConnectAckPacket ack;
    ack.header.type = Net::PacketType::ConnectAck;
    ack.header.sequence = sm_sequence++;
    ack.clientId = newId;
    ack.playerObjectId = updatedClient.playerObjectId;
    SendToClient(newId, &ack, sizeof(ack));
  }

  void ServerNet::HandleDisconnect([[maybe_unused]] const Net::DisconnectPacket& _pkt, const sockaddr_in& _addr)
  {
    uint32_t clientId = FindClientByAddress(_addr);
    if (clientId == 0)
      return;

    std::cout << "[Server] Client " << clientId << " disconnected" << std::endl;

    if (sm_onDisconnect)
      sm_onDisconnect(clientId);

    sm_clients.erase(clientId);
  }

  void ServerNet::HandleInput(const Net::ClientInputPacket& _pkt)
  {
    auto it = sm_clients.find(_pkt.clientId);
    if (it == sm_clients.end())
      return;

    it->second.lastInputSeq = _pkt.inputSequence;
    it->second.lastAck = _pkt.header.sequence;

    if (sm_onInput)
      sm_onInput(_pkt.clientId, _pkt);
  }

  void ServerNet::HandleHeartbeat(const Net::HeartbeatPacket& _pkt)
  {
    auto it = sm_clients.find(_pkt.clientId);
    if (it != sm_clients.end())
      it->second.heartbeatTimer = 0.f;
  }

  uint32_t ServerNet::FindClientByAddress(const sockaddr_in& _addr)
  {
    for (const auto& [id, client] : sm_clients)
    {
      if (client.address.sin_addr.s_addr == _addr.sin_addr.s_addr &&
          client.address.sin_port == _addr.sin_port)
        return id;
    }
    return 0;
  }
}
