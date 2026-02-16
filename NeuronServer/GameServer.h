#pragma once

#include "ServerNet.h"
#include "World.h"

namespace Neuron::Server
{
  class GameServer
  {
  public:
    static void Startup(uint16_t _port = Net::DEFAULT_PORT);
    static void Shutdown();
    static void Run();

  private:
    static void Tick(float _deltaT);
    static void OnClientConnect(uint32_t _clientId, const sockaddr_in& _addr);
    static void OnClientDisconnect(uint32_t _clientId);
    static void OnClientInput(uint32_t _clientId, const Net::ClientInputPacket& _input);
    static void BroadcastWorldState();

    inline static World    sm_world;
    inline static uint32_t sm_snapshotId  = 0;
    inline static bool     sm_running     = false;
    inline static float    sm_tickAccum   = 0.f;
    inline static float    sm_snapAccum   = 0.f;
  };
}
