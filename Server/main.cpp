#include "pch.h"
#include "Config.h"

#include "ChunkStore.h"
#include "Constants.h"
#include "Database.h"
#include "EntitySystem.h"
#include "PacketCodec.h"
#include "PacketTypes.h"
#include "ServerLog.h"
#include "SimulationEngine.h"
#include "SocketManager.h"
#include "TickProfiler.h"
#include "VoxelSystem.h"
#include "UniverseManager.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    std::atomic<bool> g_shutdownRequested{ false };

    BOOL WINAPI consoleCtrlHandler(DWORD ctrlType)
    {
        switch (ctrlType)
        {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_shutdownRequested.store(true, std::memory_order_release);
            return TRUE;
        default:
            return FALSE;
        }
    }

    // ── Connected client tracking ───────────────────────────────────────
    struct ConnectedClient
    {
        std::string    addr;
        uint16_t       port     = 0;
        uint64_t       lastTick = 0;
        Neuron::PlayerID playerId = Neuron::INVALID_PLAYER;
    };

    std::unordered_map<std::string, ConnectedClient> g_clients;
    uint32_t g_snapSequence = 0;
    Neuron::PlayerID g_nextPlayerId = 0;

    std::string makeClientKey(const std::string& addr, uint16_t port)
    {
        return addr + ":" + std::to_string(port);
    }

    /// Track a client connection and return its assigned PlayerID.
    Neuron::PlayerID trackClient(const std::string& addr, uint16_t port, uint64_t tickNum)
    {
        auto key = makeClientKey(addr, port);
        auto it = g_clients.find(key);
        if (it == g_clients.end())
        {
            auto pid = g_nextPlayerId++;
            g_clients[key] = { addr, port, tickNum, pid };
            Neuron::Server::LogInfo("Client connected: {}:{} (player {})\n", addr, port, pid);
            return pid;
        }
        else
        {
            it->second.lastTick = tickNum;
            return it->second.playerId;
        }
    }

    void broadcastSnapshot(Neuron::Server::SocketManager& socketMgr,
                           const Neuron::GameLogic::EntitySystem& entitySys,
                           uint64_t tickNum)
    {
        using namespace Neuron;

        // Build a SnapState from live entities
        SnapState snap;
        snap.serverTick  = tickNum;
        snap.entityCount = 0;

        for (const auto& e : entitySys.getAll())
        {
            if (!e.alive)
                continue;
            if (snap.entityCount >= SnapState::MAX_ENTITIES_PER_SNAP)
                break;

            auto& se = snap.entities[snap.entityCount];
            se.entityId = e.id;
            se.type     = e.type;
            se.position = e.pos;
            se.velocity = e.vel;
            se.health   = static_cast<float>(e.hp);
            se.ownerId  = e.ownerPlayerId;
            ++snap.entityCount;
        }

        auto bytes = encodePacket(snap, g_snapSequence++);

        // Send to all connected clients
        for (auto& [key, client] : g_clients)
            socketMgr.sendTo(bytes, client.addr, client.port);
    }
} // anonymous

int main(int argc, char* argv[])
{
    using namespace Neuron;
    using namespace Neuron::Server;

    // Register Windows console shutdown handler
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    // Parse command line for --config path
    std::string configPath = "config/server.yaml";
    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::string_view(argv[i]) == "--config")
            configPath = argv[i + 1];
    }

    // Initialize logging first (with default level), then reload from config
    initLogging("info");

    // Load configuration
    auto config = loadConfig(configPath);
    initLogging(config.logLevel);

    LogInfo("StarStrike Server v0.1.0\n");
    LogInfo("  Tick rate:   {} Hz\n", config.tickRateHz);
    LogInfo("  Max players: {}\n", config.maxPlayers);
    LogInfo("  Port:        {}\n", config.port);

    // Initialize database
    Database db;
    DatabaseConfig dbConfig{
        .connectionString = config.databaseUrl,
        .poolSize         = config.dbPoolSize
    };

    if (db.connect(dbConfig))
    {
        // Quick connectivity check
        auto result = db.query("SELECT 1");
        if (result.has_value())
            LogInfo("Database connectivity verified (SELECT 1)\n");
        else
            LogWarn("Database connected but SELECT 1 failed\n");
    }
    else
    {
        LogWarn("Database connection failed — running without persistence\n");
    }

    // Initialize network
    SocketManager socketMgr;
    if (!socketMgr.init(config.bindAddress, config.port))
    {
        LogFatal("Failed to bind socket — aborting");
    }

    // Initialize simulation
    SimulationEngine sim;
    TickProfiler profiler;

    // ── Initialize universe (Phase 3) ──────────────────────────────────────────
    GameLogic::UniverseManager universe;
    universe.init();
    sim.init(&universe);

    // Persistence layer
    ChunkStore chunkStore(db);
    if (db.isConnected())
    {
        chunkStore.ensureSchema();

        // Load chunks from DB for each sector
        size_t totalChunks = 0;
        for (int32_t sy = 0; sy < SECTOR_GRID_Y; ++sy)
        {
            for (int32_t sx = 0; sx < SECTOR_GRID_X; ++sx)
            {
                totalChunks += chunkStore.loadSectorChunks(
                    static_cast<uint8_t>(sx), static_cast<uint8_t>(sy),
                    universe.getVoxelSystem());
            }
        }
        LogInfo("Loaded {} chunks from database\n", totalChunks);
    }

    // Spawn test entities — 1 asteroid per sector, 1 ship at sector (0,0)
    auto& entitySys = universe.getEntitySystem();

    for (int32_t sy = 0; sy < SECTOR_GRID_Y; ++sy)
    {
        for (int32_t sx = 0; sx < SECTOR_GRID_X; ++sx)
        {
            const auto& sector = universe.getSectorManager().getSector(sx, sy);
            auto sMin = sector.minBound();

            GameLogic::Entity asteroid;
            asteroid.type  = EntityType::Asteroid;
            asteroid.pos   = { static_cast<float>(sMin.x + SECTOR_SIZE_X / 2),
                               static_cast<float>(sMin.y + SECTOR_SIZE_Y / 2),
                               static_cast<float>(SECTOR_SIZE_Z / 2) };
            asteroid.hp    = 500;
            asteroid.maxHp = 500;
            auto id = entitySys.spawnEntity(asteroid);
            LogInfo("Spawned test asteroid {} at ({}, {}, {})\n",
                       id, asteroid.pos.x, asteroid.pos.y, asteroid.pos.z);
        }
    }

    // Player test ship at sector (0,0) origin
    {
        GameLogic::Entity ship;
        ship.type         = EntityType::Ship;
        ship.ownerPlayerId = 0;
        ship.pos          = { 0.0f, 0.0f, 0.0f };
        ship.hp           = 100;
        ship.maxHp        = 100;
        auto id = entitySys.spawnEntity(ship);
        LogInfo("Spawned test ship {} at (0, 0, 0)\n", id);
    }

    LogInfo("Universe ready: {} entities, {} chunks\n",
               entitySys.liveCount(), universe.getVoxelSystem().chunkCount());

    LogInfo("Server initialized. Listening on {}:{}/udp\n",
               config.bindAddress, config.port);

    // ── Main tick loop ──────────────────────────────────────────────────────
    const auto tickInterval = std::chrono::microseconds(
        1'000'000 / config.tickRateHz);

    uint64_t tickNum = 0;
    auto nextTick = std::chrono::steady_clock::now();

    while (!g_shutdownRequested.load(std::memory_order_acquire))
    {
        profiler.beginTick();

        // Receive incoming packets
        std::vector<ReceivedPacket> packets;
        socketMgr.recvPackets(packets);

        // Track connected clients and enqueue commands for simulation
        for (const auto& pkt : packets)
        {
            auto playerId = trackClient(pkt.senderAddr, pkt.senderPort, tickNum);

            // Spawn a ship for brand-new players (playerId was just assigned)
            if (entitySys.getEntity(static_cast<EntityID>(playerId)) == nullptr)
            {
                // Check if any existing ship belongs to this player
                bool hasShip = false;
                for (const auto& e : entitySys.getAll())
                {
                    if (e.alive && e.type == EntityType::Ship &&
                        e.ownerPlayerId == playerId)
                    {
                        hasShip = true;
                        break;
                    }
                }
                if (!hasShip)
                {
                    GameLogic::Entity ship;
                    ship.type          = EntityType::Ship;
                    ship.ownerPlayerId = playerId;
                    ship.pos           = { 0.0f, 0.0f, 0.0f };
                    ship.hp            = 100;
                    ship.maxHp         = 100;
                    auto id = entitySys.spawnEntity(ship);
                    LogInfo("Spawned ship {} for player {}\n", id, playerId);
                }
            }

            // Extract CmdInput packets and feed to simulation
            if (pkt.packet.header.type == CmdInput::TYPE &&
                pkt.packet.payload.size() >= sizeof(CmdInput))
            {
                CmdInput cmd;
                std::memcpy(&cmd, pkt.packet.payload.data(), sizeof(CmdInput));
                cmd.playerId = playerId; // override with server-assigned ID
                sim.enqueueCommand(cmd);
            }
        }

        // Run simulation tick
        sim.tick(tickNum);
        universe.tick(TICK_INTERVAL_SEC, tickNum);

        // Broadcast snapshots at 20 Hz (every TICKS_PER_SNAPSHOT ticks)
        if (tickNum % TICKS_PER_SNAPSHOT == 0 && !g_clients.empty())
            broadcastSnapshot(socketMgr, entitySys, tickNum);

        // Persistence: flush voxel deltas every 1 sec, dirty chunks every 30 sec
        if (db.isConnected())
        {
            auto deltas = universe.getVoxelSystem().consumeDeltas();
            if (!deltas.empty())
                chunkStore.appendVoxelEvents(deltas);

            if (tickNum % VOXEL_EVENT_FLUSH_TICKS == 0)
                chunkStore.flushVoxelEvents();

            if (tickNum % CHUNK_FLUSH_TICKS == 0)
                chunkStore.flushDirtyChunks(universe.getVoxelSystem());
        }

        profiler.endTick();
        ++tickNum;

        // Sleep until next tick
        nextTick += tickInterval;
        auto now = std::chrono::steady_clock::now();
        if (nextTick > now)
            std::this_thread::sleep_until(nextTick);
        else
            nextTick = now; // We're behind — catch up
    }

    // ── Shutdown ────────────────────────────────────────────────────────────
    LogInfo("Shutting down (ticked {} times)...\n", tickNum);

    // Flush any remaining persistence data
    if (db.isConnected())
    {
        chunkStore.flushVoxelEvents();
        chunkStore.flushDirtyChunks(universe.getVoxelSystem());
    }

    db.disconnect();
    LogInfo("Server stopped.\n");

    return 0;
}
