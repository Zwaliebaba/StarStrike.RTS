#pragma once

#include <functional>

namespace Neuron::Client
{
  class ClientNet
  {
  public:
    static void Startup();
    static void Shutdown();
    static void Connect(const char* _serverAddress, uint16_t _port);
    static void Disconnect();
    static void Poll();
    static void SendInput(const Net::ClientInputPacket& _input);
    static void SendHeartbeat();

    [[nodiscard]] static bool IsConnected() noexcept { return sm_connected; }
    [[nodiscard]] static uint32_t GetClientId() noexcept { return sm_clientId; }
    [[nodiscard]] static ObjectId GetPlayerObjectId() noexcept { return sm_playerObjectId; }

    using SnapshotCallback = std::function<void(const Net::WorldSnapshotPacket&)>;
    static void SetSnapshotCallback(SnapshotCallback _callback) { sm_snapshotCallback = std::move(_callback); }

  private:
    static void HandleConnectAck(const Net::ConnectAckPacket& _pkt);
    static void HandleSnapshot(const uint8_t* _data, int _size);

    inline static SOCKET     sm_socket          = INVALID_SOCKET;
    inline static sockaddr_in sm_serverAddr     = {};
    inline static bool       sm_connected       = false;
    inline static bool       sm_connecting      = false;
    inline static uint32_t   sm_clientId        = 0;
    inline static ObjectId   sm_playerObjectId  = INVALID_OBJECT_ID;
    inline static uint16_t   sm_sequence        = 0;
    inline static float      sm_connectTimer    = 0.f;
    inline static SnapshotCallback sm_snapshotCallback;
  };
}
