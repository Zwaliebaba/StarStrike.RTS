#pragma once

#include "NetProtocol.h"
#include <unordered_map>
#include <functional>

namespace Neuron::Server
{
  struct ClientConnection
  {
    sockaddr_in address       = {};
    uint32_t    clientId      = 0;
    ObjectId    playerObjectId = INVALID_OBJECT_ID;
    uint16_t    lastAck       = 0;
    uint32_t    lastAckBits   = 0;
    uint32_t    lastInputSeq  = 0;
    float       heartbeatTimer = 0.f;
  };

  class ServerNet
  {
  public:
    static void Startup(uint16_t _port);
    static void Shutdown();
    static void Poll();
    static void SendToClient(uint32_t _clientId, const void* _data, int _size);
    static void BroadcastToAll(const void* _data, int _size);

    using ConnectHandler    = std::function<void(uint32_t clientId, const sockaddr_in& addr)>;
    using DisconnectHandler = std::function<void(uint32_t clientId)>;
    using InputHandler      = std::function<void(uint32_t clientId, const Net::ClientInputPacket& input)>;

    static void SetConnectHandler(ConnectHandler _handler)       { sm_onConnect = std::move(_handler); }
    static void SetDisconnectHandler(DisconnectHandler _handler) { sm_onDisconnect = std::move(_handler); }
    static void SetInputHandler(InputHandler _handler)           { sm_onInput = std::move(_handler); }

    static const std::unordered_map<uint32_t, ClientConnection>& GetClients() noexcept { return sm_clients; }
    static void SetPlayerObjectId(uint32_t _clientId, ObjectId _objectId);

  private:
    static void HandleConnect(const sockaddr_in& _addr);
    static void HandleDisconnect(const Net::DisconnectPacket& _pkt, const sockaddr_in& _addr);
    static void HandleInput(const Net::ClientInputPacket& _pkt);
    static void HandleHeartbeat(const Net::HeartbeatPacket& _pkt);

    static uint32_t FindClientByAddress(const sockaddr_in& _addr);

    inline static SOCKET sm_socket = INVALID_SOCKET;
    inline static std::unordered_map<uint32_t, ClientConnection> sm_clients;
    inline static uint32_t sm_nextClientId = 1;
    inline static uint16_t sm_sequence     = 0;

    inline static ConnectHandler    sm_onConnect;
    inline static DisconnectHandler sm_onDisconnect;
    inline static InputHandler      sm_onInput;
  };
}
