#include "pch.h"
#include "Config.h"

#include "ChunkStore.h"
#include "Constants.h"
#include "Database.h"
#include "EntitySystem.h"
#include "ServerLog.h"
#include "SimulationEngine.h"
#include "SocketManager.h"
#include "TickProfiler.h"
#include "VoxelSystem.h"
#include "UniverseManager.h"

#include <atomic>
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

        // Run simulation tick
        sim.tick(tickNum);
        universe.tick(TICK_INTERVAL_SEC, tickNum);

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
