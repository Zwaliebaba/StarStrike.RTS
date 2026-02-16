#include "pch.h"
#include "GameServer.h"

namespace Neuron::Server
{
  void GameServer::Startup(uint16_t _port)
  {
    sm_world.Startup(WORLD_DEFAULT_SIZE);

    // Spawn some static asteroids for visual interest
    (void)sm_world.SpawnObject(WorldObjectType::Asteroid, 0, {100.f, 0.f, 100.f});
    (void)sm_world.SpawnObject(WorldObjectType::Asteroid, 0, {-150.f, 0.f, 200.f});
    (void)sm_world.SpawnObject(WorldObjectType::Asteroid, 0, {300.f, 0.f, -100.f});

    ServerNet::SetConnectHandler(OnClientConnect);
    ServerNet::SetDisconnectHandler(OnClientDisconnect);
    ServerNet::SetInputHandler(OnClientInput);
    ServerNet::Startup(_port);

    sm_snapshotId = 0;
    sm_tickAccum = 0.f;
    sm_snapAccum = 0.f;
    sm_running = true;

    std::cout << "[Server] Game server started" << std::endl;
  }

  void GameServer::Shutdown()
  {
    sm_running = false;
    ServerNet::Shutdown();
    sm_world.Shutdown();
    std::cout << "[Server] Game server shutdown" << std::endl;
  }

  void GameServer::Run()
  {
    using Clock = std::chrono::high_resolution_clock;
    auto previousTime = Clock::now();

    std::cout << "[Server] Running... Press Ctrl+C to stop." << std::endl;

    while (sm_running)
    {
      auto currentTime = Clock::now();
      float deltaT = std::chrono::duration<float>(currentTime - previousTime).count();
      previousTime = currentTime;

      // Cap delta to avoid spiral of death
      if (deltaT > 0.25f)
        deltaT = 0.25f;

      sm_tickAccum += deltaT;
      sm_snapAccum += deltaT;

      // Process network
      ServerNet::Poll();

      // Fixed-step simulation at 20 Hz
      while (sm_tickAccum >= Net::TICK_INTERVAL)
      {
        Tick(Net::TICK_INTERVAL);
        sm_tickAccum -= Net::TICK_INTERVAL;
      }

      // Snapshot broadcast at 10 Hz
      if (sm_snapAccum >= Net::SNAPSHOT_INTERVAL)
      {
        BroadcastWorldState();
        sm_snapAccum -= Net::SNAPSHOT_INTERVAL;
      }

      // Sleep to avoid spinning
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  void GameServer::Tick(float _deltaT)
  {
    sm_world.Update(_deltaT);
  }

  void GameServer::OnClientConnect(uint32_t _clientId, [[maybe_unused]] const sockaddr_in& _addr)
  {
    // Alternate ship classes for variety
    uint8_t shipClass = static_cast<uint8_t>(_clientId % static_cast<uint32_t>(ShipClass::Count));

    // Spawn position offset per client
    float offsetX = static_cast<float>(_clientId) * 30.0f - 50.0f;
    ObjectId objId = sm_world.SpawnObject(WorldObjectType::Ship, shipClass, {offsetX, 0.f, 0.f}, _clientId);

    ServerNet::SetPlayerObjectId(_clientId, objId);
    std::cout << "[Server] Spawned ship (class=" << static_cast<int>(shipClass)
              << ") for client " << _clientId << " objectId=" << objId << std::endl;
  }

  void GameServer::OnClientDisconnect(uint32_t _clientId)
  {
    auto& clients = ServerNet::GetClients();
    auto it = clients.find(_clientId);
    if (it != clients.end())
    {
      sm_world.RemoveObject(it->second.playerObjectId);
    }
    std::cout << "[Server] Removed objects for client " << _clientId << std::endl;
  }

  void GameServer::OnClientInput(uint32_t _clientId, const Net::ClientInputPacket& _input)
  {
    auto& clients = ServerNet::GetClients();
    auto it = clients.find(_clientId);
    if (it == clients.end())
      return;

    ObjectId objId = it->second.playerObjectId;

    switch (_input.inputType)
    {
    case Net::InputType::MoveTo:
      sm_world.SetObjectTarget(objId, _input.targetPosition);
      break;
    case Net::InputType::Stop:
      sm_world.StopObject(objId);
      break;
    default:
      break;
    }
  }

  void GameServer::BroadcastWorldState()
  {
    const auto& objects = sm_world.GetObjects();

    Net::WorldSnapshotPacket snapshot;
    snapshot.header.type = Net::PacketType::WorldSnapshot;
    snapshot.header.sequence = static_cast<uint16_t>(sm_snapshotId & 0xFFFF);
    snapshot.snapshotId = sm_snapshotId++;
    snapshot.objectCount = 0;

    for (const auto& [id, obj] : objects)
    {
      if (snapshot.objectCount >= Net::MAX_SNAPSHOT_OBJECTS)
        break;

      auto& sd = snapshot.objects[snapshot.objectCount];
      sd.objectId   = obj.state.id;
      sd.objectType = obj.state.type;
      sd.subclass   = obj.state.subclass;
      sd.flags      = obj.state.flags;
      sd.position   = obj.state.position;
      sd.velocity   = obj.state.velocity;
      sd.yaw        = obj.state.yaw;
      sd.hitpoints  = obj.state.hitpoints;
      snapshot.objectCount++;
    }

    // Include per-client lastProcessedInput
    for (const auto& [clientId, conn] : ServerNet::GetClients())
    {
      snapshot.lastProcessedInput = conn.lastInputSeq;
      ServerNet::SendToClient(clientId, &snapshot, static_cast<int>(snapshot.GetSize()));
    }
  }
}
