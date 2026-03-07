# StarStrike: Voxel-Based Isometric RTS MMO Space Game — Full Architecture

**Last Updated:** March 2026  
**Status:** Architecture Design Complete  
**Target:** 8–12 week MVP build  

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [System Context Diagram](#system-context-diagram)
3. [Client Architecture](#client-architecture)
4. [Server Architecture](#server-architecture)
5. [Networking Model](#networking-model)
6. [Voxel World Design](#voxel-world-design)
7. [Persistence Strategy](#persistence-strategy)
8. [Rendering Pipeline](#rendering-pipeline)
9. [Deployment Architecture](#deployment-architecture)
10. [Observability](#observability)
11. [Repository Layout](#repository-layout)
12. [MVP Plan](#mvp-plan)
13. [Risks & Mitigations](#risks--mitigations)

---

## Architecture Overview

### Design Philosophy

StarStrike combines **persistent voxel terrain**, **real-time multiplayer combat**, and **MMO scalability** in a server-authoritative architecture:

- **Authoritative Server**: All gameplay, voxel destruction, and combat resolved server-side
- **Client Prediction**: Ships and projectiles predicted locally; reconciled on server state
- **Persistent World**: Voxel destruction permanent for all players; stored in PostgreSQL
- **Sector-Based Partitioning**: 4×4 grid of sectors initially; each sector a bounded gameplay volume
- **60 Hz Tick Rate**: 16.67 ms simulation ticks for responsive combat and predictable physics

### Technical Stack

| Component | Technology | Rationale |
|-----------|-----------|-----------|
| **Client OS** | Windows 10+ (x64) | Desktop focus; future Vulkan for Linux |
| **Client Rendering** | DirectX 12 | Modern GPU API; low-latency; supports compute shaders for voxel ops |
| **Client UI** | D2D1 (Direct2D) + custom widget layer | Vector UI, low overhead, integrates with DX12 |
| **Server Language** | C++23 (MSVC) | Performance, determinism, memory control |
| **Server Transport** | UDP (custom) over IPv4 | Low latency; packet loss handled via resend/ACK logic |
| **Persistence Layer** | PostgreSQL 14+ (relational) | ACID guarantees for voxel chunks and entity snapshots |
| **Object Storage** | PostgreSQL BYTEA for chunk data | Simplifies deployment; alternative: S3 for future scale |
| **Deployment** | Docker (Windows Server Core) | Single-container MVP; orchestration-ready |
| **Scripting** | YAML config + Lua (optional, for AI tuning) | Designer-facing parameters |

### Core Metrics (MVP)

| Metric | Target |
|--------|--------|
| Concurrent players | 50 |
| Sector size | 512×512×256 voxels ≈ 67 MB uncompressed per sector |
| Chunk size | 32×32×32 voxels; 4,096 voxels per chunk |
| Max ships per player | 5 |
| Tick rate | 60 Hz (16.67 ms) |
| Client update rate | 20 Hz snapshots (50 ms) + client prediction |
| Voxel streaming latency | < 500 ms per chunk |
| Typical player bandwidth | 50–200 kbps up/down |
| Server memory | 4 GB (scalable to 16 GB) |

---

## System Context Diagram

```mermaid
graph TB
    Player["👤 Player"]
    Client["Client<br/>Win32 + DX12"]
    Network["Network<br/>UDP Port 7777"]
    Server["Server<br/>C++ Sim Loop"]
    DB["PostgreSQL<br/>Voxels + Entities"]
    Cache["Redis<br/>Session Cache"]
    Metrics["Prometheus<br/>Metrics"]
    Container["Docker Container<br/>Windows Server Core"]
    
    Player -->|"Keyboard/Mouse"| Client
    Client -->|"Commands<br/>50 ms"| Network
    Network -->|"Snapshots<br/>200–500 kbps"| Client
    Network -->|"All traffic"| Server
    Server -->|"Query/Update<br/>~20 ms latency"| DB
    Server -->|"Session state"| Cache
    Server -->|"Metrics"| Metrics
    Server -->|"Runs inside"| Container
    
    style Client fill:#0066cc,stroke:#fff,color:#fff
    style Server fill:#cc3300,stroke:#fff,color:#fff
    style DB fill:#0a8c42,stroke:#fff,color:#fff
    style Network fill:#ff9900,stroke:#000
```

---

## Client Architecture

### Module Breakdown

```mermaid
graph TB
    subgraph Rendering["Rendering Layer"]
        DX12["DX12 Device<br/>Command Queues"]
        VoxelRend["Voxel Renderer<br/>Greedy Meshing + LOD"]
        EntityRend["Entity Renderer<br/>Ships, FX, Projectiles"]
        UIRend["UI Renderer<br/>HUD, Menus"]
        PostProcess["Post-Process<br/>Pixelation, Glow"]
    end
    
    subgraph Simulation["Local Simulation"]
        ShipSim["Ship Predictor<br/>Pos, Vel, Rotation"]
        ProjectileSim["Projectile Predictor<br/>Ballistics"]
        InputCmdProc["Input Processor<br/>Commands Queue"]
    end
    
    subgraph Network["Networking"]
        UDPSocket["UDP Socket<br/>Recv/Send"]
        PacketCodec["Packet Codec<br/>Serialize/Deserialize"]
        CommandQ["Outbound Cmd Queue"]
        SnapshotDecode["Snapshot Decoder<br/>Entity + Voxel deltas"]
    end
    
    subgraph World["World State"]
        VoxelCache["Voxel Chunk Cache<br/>GPU + CPU RAM"]
        EntityList["Entity List<br/>Ships, asteroids, timers"]
        ChunkStream["Chunk Streamer<br/>Interest-based loading"]
    end
    
    subgraph Assets["Assets"]
        VoxelMeshes["Voxel Mesh Pool<br/>Atlas + LODs"]
        Shaders["Shaders<br/>Voxel, Entity, UI"]
        Textures["Textures<br/>Diffuse, Normal, Emissive"]
    end
    
    subgraph Input["Input"]
        Keyboard["Keyboard Events"]
        Mouse["Mouse Events"]
        Camera["Camera Controller<br/>Isometric Pan/Zoom"]
    end
    
    Keyboard --> InputCmdProc
    Mouse --> InputCmdProc
    InputCmdProc --> CommandQ
    InputCmdProc --> Camera
    
    CommandQ --> PacketCodec
    PacketCodec --> UDPSocket
    UDPSocket --> SnapshotDecode
    SnapshotDecode --> EntityList
    SnapshotDecode --> VoxelCache
    
    ShipSim --> EntityRend
    ProjectileSim --> EntityRend
    EntityList --> EntityRend
    
    VoxelCache --> VoxelMeshes
    VoxelMeshes --> VoxelRend
    
    Camera --> VoxelRend
    Camera --> EntityRend
    
    VoxelRend --> PostProcess
    EntityRend --> PostProcess
    UIRend --> PostProcess
    
    PostProcess --> DX12
    
    style Rendering fill:#0066cc,stroke:#fff,color:#fff
    style Simulation fill:#ff6600,stroke:#000
    style Network fill:#ff9900,stroke:#000
    style World fill:#0a8c42,stroke:#fff,color:#fff
    style Input fill:#6600cc,stroke:#fff,color:#fff
```

### Key Components

#### 1. **Rendering Layer**

**DX12 Device**
- Manages command allocators, lists, and queues
- Double-buffered backbuffer + depth
- Per-frame descriptor heap management
- Synchronization: fences + event-based timing

**Voxel Renderer**
- Consumes voxel chunks from GPU VRAM cache
- Generates optimized meshes (greedy meshing algorithm)
- LOD strategy:
  - **LOD0**: Full detail for chunks within 8 chunks of camera
  - **LOD1**: Half resolution for 8–16 chunk range
  - **LOD2**: Quarter resolution for 16–32 chunk range
  - **LOD3**: Invisible beyond 32 chunks
- Renders to intermediate render target (see **Pixelation post-process** below)
- Submits batch draw calls sorted by distance (back-to-front)

**Entity Renderer**
- Renders ships and projectiles over voxel layer
- Billboard particles for explosions, mining effects
- Emissive layer for glowing shield/engine effects

**UI Renderer**
- HUD: health, ammo, radar, minimap
- Menus: pause, settings, chat
- Text: world coords for floaty damage numbers (optional MVP)
- Rendered to separate RT, composited over scene

**Post-Process**
- Low-res intermediate RT target (e.g., 640×360 or 800×450)
- Pixelation via nearest-neighbor upscale to native resolution
- Glow/bloom for emissive fissures and energy effects
- Tone mapping for retro aesthetic

#### 2. **Local Simulation**

**Ship Predictor**
- Stores local copy of all visible ships (extrapolated from last snapshot)
- On each frame: apply velocity, update rotation, integrate acceleration
- When server snapshot arrives: blend new state (or hard snap if far out of sync)
- Supports dead reckoning for smooth movement between 20 Hz snapshots

**Projectile Predictor**
- Similar to ship predictor, but simpler (ballistic trajectory only)
- Despawns locally after expected TTL expires
- Server authoritative on hit; client shows predicted impact zone

**Input Processor**
- Polls keyboard and mouse each frame
- Translates to command structs (e.g., `MoveTo(x, y, z)`, `AttackTarget(entityID)`)
- Queues for transmission; does **not** immediately apply to local ship (waits for server ack)

#### 3. **Networking**

**UDP Socket (WinSock2)**
- Uses **Windows Sockets 2 (WinSock2)** API for cross-version Windows compatibility
- Non-blocking I/O via `WSAEventSelect()` or `WSAAsyncSelect()`
- Binds to local port (ephemeral, assigned by OS)
- Sends to server IP:7777
- Platform: Windows 10+ native support (winsock2.h, link ws2_32.lib)

**Packet Codec**
- Binary serialization (hand-written, no IDL overhead)
- Compression: optional Snappy or LZ4 for large snapshots
- Message types:
  - `CMD_INPUT`: Player commands (position, targeting)
  - `CMD_CHAT`: Text chat
  - `SNAP_ENTITIES`: Entity states (ships, asteroids, projectiles)
  - `SNAP_VOXELS`: Chunk deltas (added/removed voxels)
  - `ACK`: Acknowledgment (packet loss detection)

**Chunk Streamer**
- Tracks which chunks are loaded and visible
- Requests chunks based on camera frustum + interest radius
- Prioritizes near chunks; loads far chunks async to avoid stalls
- Deletes chunks no longer needed (LRU if memory constrained)

#### 4. **World State**

**Voxel Chunk Cache**
- 2-tier:
  - **GPU VRAM**: Compressed mesh + texture atlas (VB + IB locked, read-no-access)
  - **CPU RAM**: Uncompressed voxel data for collision queries (sparse representation)
- Max capacity: ~200 chunks in MVP (32 × 32 × 32 × 4 bytes ≈ 512 MB for full sector)
- Eviction: LRU by last-rendered timestamp

**Entity List**
- Struct-of-arrays for cache efficiency:
  - `vector<EntityID> ids`
  - `vector<Vec3> positions`
  - `vector<Vec3> velocities`
  - `vector<uint16> shipsTypes` (fighter, carrier, etc.)
  - `vector<uint8> faction` (player ID)
- Supports up to 10,000 entities (far more than needed for MVP)

**Chunk Streaming Manager**
- Interest region: sphere or box around camera (e.g., 8-chunk radius)
- Triggers periodic chunk requests to server
- Applies received chunk deltas (voxel removals) in real-time

#### 5. **Input System**

**Camera Controller**
- Isometric view: fixed 45° pitch, 45° yaw (or artist-configurable)
- Pan: WASD or mouse drag
- Zoom: Mouse wheel; clamped to 4x–64x
- Target follow: optional lock-on to selected entity
- Frustum culling: only render chunks within view cone

#### 6. **Asset System**

**Voxel Mesh Pool**
- Pre-generated cubes + quads in atlas
- Greedy meshing reduces vertex count by 40–60% per chunk
- Mesh stored in GPU VB/IB; CPU keeps sparse grid for collision
- Color palette: 256 base colors + tint/AO per face

**Shaders**
- Vertex: world pos → screen pos, UV → texel, normal → lighting
- Pixel: sample texel, apply emissive if ID matches fissure set, output linear + bloom
- Post-process: box blur (glow), nearest-neighbor upsample (pixelation)

**Textures**
- Single atlas: 2048×2048 RGBA (or two atlases if needed)
- Diffuse: base color
- Normal: per-face normal for lighting
- Emissive: 1-bit mask + glow intensity for fissures

---

## Server Architecture

### High-Level Flow

```
┌─────────────────────────────────────────────────────┐
│ Main Loop (60 Hz, 16.67 ms per tick)                │
├─────────────────────────────────────────────────────┤
│ 1. Recv packets from all connected clients           │
│ 2. Process commands (movement, combat, mining, chat) │
│ 3. Update voxel world (destruction, regeneration)    │
│ 4. Simulate ships, projectiles, effects              │
│ 5. Collision detection                               │
│ 6. Advance state (process wins, losses, resources)   │
│ 7. Prepare snapshots (only send deltas + new events) │
│ 8. Send snapshots to interested clients              │
│ 9. Log metrics (tick duration, player count)         │
└─────────────────────────────────────────────────────┘
```

### Module Architecture

```mermaid
graph TB
    subgraph TransportLayer["Transport & Input"]
        Socket["UDP Socket<br/>Recv Buffer"]
        PacketDecoder["Packet Decoder"]
        RateLimiter["Rate Limiter<br/>Per-client"]
        CommandQ["Command Queue<br/>Per-tick"]
    end
    
    subgraph SimulationEngine["Simulation Engine"]
        TickPhase1["Phase 1: Input<br/>Process all commands"]
        TickPhase2["Phase 2: Voxel Update<br/>Apply destruction"]
        TickPhase3["Phase 3: Physics<br/>Integrate ships/projectiles"]
        TickPhase4["Phase 4: Combat<br/>Process hits, damage"]
        TickPhase5["Phase 5: Mining<br/>Extract resources"]
        TickPhase6["Phase 6: Effects<br/>Create FX entities"]
    end
    
    subgraph WorldManager["World Manager"]
        SectorMgr["Sector Manager<br/>Partition & bounds"]
        EntityMgr["Entity System<br/>CRUD, lifecycle"]
        VoxelMgr["Voxel System<br/>Chunk + Delta buffer"]
        PhysicsEngine["Physics Engine<br/>AABB, raycasts"]
    end
    
    subgraph PersistenceLayer["Persistence"]
        ChunkStore["Chunk Store<br/>PostgreSQL"]
        EntityStore["Entity Store<br/>PostgreSQL"]
        TransactionLog["Transaction Log<br/>Voxel events"]
    end
    
    subgraph InterestMgmt["Interest Management"]
        VisibilityCalc["Visibility Calc<br/>Frustum + distance"]
        SnapshotFilter["Snapshot Filter<br/>Per-player view"]
        ChunkRequestMgr["Chunk Request Mgr<br/>Stream chunks to clients"]
    end
    
    subgraph OutputLayer["Output & Network"]
        SnapshotBuilder["Snapshot Builder<br/>Entity + voxel deltas"]
        PacketEncoder["Packet Encoder<br/>Serialize"]
        SendBuffer["Send Buffer<br/>Per-client"]
        ThrottleOut["Output Throttle<br/>20 Hz max"]
    end
    
    subgraph ObservabilityLog["Observability"]
        Logging["Logging<br/>File + stdout"]
        Metrics["Metrics<br/>Prometheus"]
        Profiling["Profiling<br/>Tick timing"]
    end
    
    Socket --> PacketDecoder
    PacketDecoder --> RateLimiter
    RateLimiter --> CommandQ
    
    CommandQ --> TickPhase1
    TickPhase1 --> TickPhase2
    TickPhase2 --> TickPhase3
    TickPhase3 --> TickPhase4
    TickPhase4 --> TickPhase5
    TickPhase5 --> TickPhase6
    
    TickPhase1 --> EntityMgr
    TickPhase2 --> VoxelMgr
    TickPhase3 --> PhysicsEngine
    TickPhase4 --> PhysicsEngine
    
    TickPhase6 --> EntityMgr
    
    VoxelMgr --> ChunkStore
    EntityMgr --> EntityStore
    PhysicsEngine --> EntityMgr
    
    TickPhase6 --> SnapshotFilter
    EntityMgr --> SnapshotFilter
    VoxelMgr --> SnapshotFilter
    
    SnapshotFilter --> SnapshotBuilder
    SnapshotBuilder --> PacketEncoder
    PacketEncoder --> SendBuffer
    SendBuffer --> ThrottleOut
    ThrottleOut --> Socket
    
    TickPhase1 --> Logging
    TickPhase6 --> Metrics
    PhysicsEngine --> Profiling
    
    style TransportLayer fill:#0066cc,stroke:#fff,color:#fff
    style SimulationEngine fill:#ff6600,stroke:#000
    style WorldManager fill:#0a8c42,stroke:#fff,color:#fff
    style PersistenceLayer fill:#996633,stroke:#fff,color:#fff
    style InterestMgmt fill:#cc00cc,stroke:#fff,color:#fff
    style OutputLayer fill:#ffcc00,stroke:#000
```

### Core Subsystems

#### 1. **Network Transport**

**UDP Socket**
- Listens on 0.0.0.0:7777
- Non-blocking recv; each loop:
  - Recv up to 100 datagrams (configurable)
  - Discard if malformed or from unknown client
  - Enqueue packet metadata (source IP:port, timestamp)

**Packet Decoder**
- Validates packet structure (header magic, CRC)
- Deserializes message(s) from binary format
- Extracts command(s) and metadata
- Drops duplicates (sequence numbers)

**Rate Limiter**
- Per-client max 100 commands/sec (or configurable burst)
- Rejects excess; responds with "rate limit exceeded" (informational)
- Prevents input-flood attacks

**Command Queue**
- Ordered list of processed commands for this tick
- Each command includes player ID, timestamp, action (e.g., MoveTo, AttackTarget)
- Cleared at end of tick

#### 2. **Simulation Engine**

**Phase 1: Input Processing**
- For each command in queue:
  - Validate command (player owns ship, target exists, in range)
  - Apply to entity state (set target, update desired velocity)
  - Reject invalid commands silently (log with player ID)

**Phase 2: Voxel Update**
- Process queued voxel destruction events:
  - Remove voxel from sector's chunk
  - Mark chunk "dirty" for mesh rebuild
  - Write delta to transaction log (for persistence)
- Optional: block mining if voxel regeneration is enabled (not MVP)

**Phase 3: Physics Integration**
- For each ship:
  - `vel += accel * dt` (thrusters and drag)
  - `pos += vel * dt`
  - `rot += angvel * dt`
  - Clamp pos to sector bounds (or teleport to adjacent sector)
- For each projectile:
  - `pos += vel * dt` (gravity-free in space)
  - Check TTL; despawn if expired
  - Perform AABB collision query for hit detection (see Phase 4)

**Phase 4: Combat**
- For each projectile with collision hit:
  - Check if target still exists and is vulnerable
  - Apply damage to target
  - Create explosion FX entity
  - Queue "entity_took_damage" event (for logging)
- Despawn projectile

**Phase 5: Mining**
- For each ship with mining beam active:
  - Cast ray from ship toward target asteroid
  - Check for voxel intersection
  - Remove voxel; spawn "mined resource" entity (drifts toward ship)
  - Update pilot's resource count

**Phase 6: Effects & Cleanup**
- Process pending FX spawns (explosions, mining sparks)
- Create effect entities (position, lifetime)
- Clean up expired entities (projectiles, effects)

#### 3. **World Manager**

**Sector Manager**
- Maintains 16 sectors (4×4 grid for MVP)
- Each sector: 512×512×256 voxels = bounds
- Sector ID = (grid_x, grid_y); computed from world pos
- Handles sector transitions (player moves across boundary)

**Entity System (ECS-lite)**
- Array-of-structs in separate vectors for cache coherency:
  ```cpp
  struct Entity {
    EntityID id;
    Vec3 pos, vel, accel;
    Quat rot;
    uint8 type; // SHIP, ASTEROID, PROJECTILE, RESOURCE
    uint16 ownerPlayerID;
    uint32 hp, maxHP;
    uint32 targetEntityID; // EntityID::INVALID if none
    uint64 lastUpdateTick;
  };
  
  vector<Entity> entities;
  unordered_map<EntityID, size_t> entityIndex; // fast lookup
  ```
- CRUD operations: `SpawnEntity()`, `DestroyEntity()`, `GetEntity()`
- Tick updates: iterate entities array once per phase

**Voxel System**
- Chunk-based storage:
  ```cpp
  struct VoxelChunk {
    Vec3i min; // sector-local coords
    uint8 voxels[32][32][32]; // 0 = empty, 1–255 = terrain type
    bool dirty; // mesh rebuild needed
    uint64 lastModified; // timestamp
  };
  
  unordered_map<ChunkID, VoxelChunk> chunks;
  ```
- Delta buffer (per-tick changes):
  ```cpp
  struct VoxelDelta {
    Vec3i worldPos;
    uint8 newValue;
    uint64 tick;
  };
  
  vector<VoxelDelta> deltaBuffer; // flushed to db every N ticks
  ```

**Physics Engine**
- AABB collision:
  - Ships: 10×10×10 units
  - Asteroids: voxel-defined (use bounding box)
  - Projectiles: 1×1×1 units (point-like)
- Raycasts: for weapon impact and mining beam sight lines
- No physics simulation (no gravity, rigid body dynamics)

#### 4. **Persistence Layer**

**PostgreSQL Schema**

```sql
-- Voxel chunks
CREATE TABLE voxel_chunks (
  chunk_id BYTEA PRIMARY KEY,         -- (sector_x, sector_y, chunk_x, chunk_y, chunk_z)
  sector_id VARCHAR(10),               -- e.g., "0,0"
  voxel_data BYTEA,                    -- zstd-compressed chunk data
  version INT,                         -- incremented on update
  modified_at TIMESTAMP DEFAULT NOW()
);

-- Players & fleets
CREATE TABLE players (
  player_id INT PRIMARY KEY,
  username VARCHAR(64) UNIQUE,
  faction INT,                         -- 0 = neutral, 1+ = known factions
  resource_credits INT DEFAULT 0,
  last_login TIMESTAMP,
  created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE ships (
  ship_id INT PRIMARY KEY,
  player_id INT REFERENCES players(player_id),
  ship_type INT,                       -- 0 = fighter, 1 = carrier, etc.
  sector_id VARCHAR(10),               -- denormalized for quick lookup
  pos_x REAL, pos_y REAL, pos_z REAL,
  hp INT,
  max_hp INT,
  resource_cargo INT,
  updated_at TIMESTAMP DEFAULT NOW()
);

-- Voxel delta log (audit trail)
CREATE TABLE voxel_events (
  event_id BIGSERIAL PRIMARY KEY,
  chunk_id BYTEA,
  world_x INT, world_y INT, world_z INT,
  old_voxel INT, new_voxel INT,
  player_id INT REFERENCES players(player_id),
  timestamp TIMESTAMP DEFAULT NOW()
);

CREATE INDEX idx_voxel_events_chunk ON voxel_events(chunk_id);
CREATE INDEX idx_chunks_sector ON voxel_chunks(sector_id);
```

**Write Patterns**
- Chunks: buffered, flushed every 30 seconds or on explicit \`SaveChunk()\`
- Ships: updated after each tick (or batched every 5 ticks if no state change)
- Events: appended to voxel_events on each destruction (transactional integrity)

**Read Patterns**
- On player login: load ships for that player
- On chunk load request: fetch chunk from voxel_chunks; decompress
- On server start: load all chunk metadata from voxel_chunks table

#### 5. **Interest Management**

**Visibility Calculation**
- For each player, compute visible entity set:
  - Entities within distance D (e.g., 16 chunks ≈ 512 units) are "near"
  - Entities within distance D×2 are "far" (lower update frequency)
  - Entities beyond D×2 are culled
- Distance computed per-tick; cached until next visibility update (every 200 ms)

**Snapshot Filtering**
- Per-client: build snapshot containing only:
  - Entities in visibility range
  - Voxel chunks in streamed region
  - Chat messages from all players (always broadcast)
- Encode only changed entity state (delta compression)

**Chunk Request Manager**
- When client requests chunk: check if already loaded
- If not: fetch from DB; send to client with priority queue
- Prioritize chunks closest to player camera

#### 6. **Output & Network**

**Snapshot Builder**
- Per-client, per-tick (at 20 Hz frequency, not 60 Hz):
  - Encode entity deltas: pos/vel/rot/hp changes
  - Encode voxel deltas: removed chunks, new chunks, chunk edits
  - Encode events: damage taken, resource gained, etc.
  - Total size: 100–500 bytes per player (depends on activity)

**Packet Encoder**
- Serializes snapshot to binary:
  - Header: tick number, client ID, flag (snapshot/ack/event)
  - Entity deltas: compressed (delta-encoded positions)
  - Voxel deltas: compressed (chunk IDs + RLE voxel data)
  - Event list: variable-length records
- Optional: apply LZ4 compression if > 500 bytes

**Output Throttle**
- Snapshots sent at 20 Hz (50 ms intervals) max
- Reduces bandwidth and allows client-side interpolation
- Server queues all snapshots; sends up to 3 per loop if backlog

---

## Networking Model

### Message Types & Frequencies

#### **Inbound (Client → Server)**

| Message | Freq | Size | Purpose |
|---------|------|------|---------|
| `CMD_INPUT` | 60 Hz | 24 bytes | Player action (move, attack, mine) |
| `CMD_CHAT` | On-demand | 64–256 bytes | Text message |
| `CMD_ACK` | 20 Hz | 4 bytes | Acknowledge received snapshot |
| `CMD_REQUEST_CHUNK` | Async | 8 bytes | Request voxel chunk |

**Example `CMD_INPUT` (24 bytes):**
```
u32 cmd_type = CMD_INPUT
u32 player_id
u8 action (1=move, 2=attack, 3=mine, 4=stop)
f32 target_x, target_y, target_z
u16 target_entity_id (0xFFFF if none)
```

**Example `CMD_ACK`:**
```
u32 cmd_type = CMD_ACK
u32 last_snapshot_tick
```

#### **Outbound (Server → Client)**

| Message | Freq | Size | Purpose |
|---------|------|------|---------|
| `SNAP_STATE` | 20 Hz | 200–800 bytes | Entity + voxel deltas |
| `SNAP_CHUNK` | Async | 4 KB–32 KB | Voxel chunk data |
| `EVENT_CHAT` | On broadcast | 64–256 bytes | Chat message from other player |
| `EVENT_DAMAGE` | Per hit | 12 bytes | Entity took X damage |
| `PING` | 5 Hz | 4 bytes | Server still alive |

**Example `SNAP_STATE` (variable):**
```
u32 cmd_type = SNAP_STATE
u64 tick_number
u16 num_entity_deltas
  [for each delta]
    u32 entity_id
    u8 field_mask (bit 0=pos, 1=rot, 2=vel, 3=hp, 4=despawn)
    [if pos] f32 x, y, z
    [if rot] u16 yaw, pitch (quantized 0–65535)
    [if vel] i16 vx, vy, vz (quantized)
    [if hp] u16 hp_current, hp_max
    [if despawn] <end>
u16 num_voxel_deltas
  [for each voxel delta]
    i32 world_x, world_y, world_z
    u8 new_voxel_type
```

### Traffic Budgets

**Inbound per player:**
- Input: 1 cmd/frame × 24 bytes = 1.44 KB/s @ 60 Hz input
- ACK: 1 msg/50 ms × 4 bytes = 0.08 KB/s
- **Total: ~1.5 KB/s per player (worst case)**

**Outbound per player:**
- Snapshot: 1 msg/50 ms (20 Hz) × 250 bytes avg = 5 KB/s
- Chunk requests: 1 chunk/2 sec × 16 KB = 8 KB/s
- Chat/events: ~1 KB/s
- **Total: ~14 KB/s per player**

**Aggregate (50 players):**
- Inbound: 75 KB/s
- Outbound: 700 KB/s
- Requires Gigabit NIC; well within VM capacity

### Packet Loss Handling

**Client-side:**
- Resend `CMD_INPUT` if no `SNAP_STATE` received for 250 ms
- Buffer up to 3 pending commands (sequence number)
- Accept snapshots out-of-order; use snapshot tick to sort

**Server-side:**
- On duplicate `CMD_INPUT`: check sequence; drop if already processed
- On missing `CMD_ACK`: assume packet loss; continue normally (no retransmit needed for one-way UDP)
- Discard packets from unknown clients after 60 seconds of inactivity

### Tick Synchronization

- Server publishes snapshot tick number in each `SNAP_STATE`
- Client advances local tick from input, but does not assume server ack
- When snapshot arrives: reconcile if client tick ahead (snap to server); rewind if behind (lerp)
- Maintains ~16–33 ms latency tolerance (1–2 frames)

---

## Voxel World Design

### Chunk Format & Storage

#### **Chunk Layout**
- Chunk size: **32×32×32 voxels**
- World space: chunks aligned to 32-unit boundaries
- Chunk ID: `uint64` = `(sector_x << 40) | (sector_y << 24) | (chunk_x << 16) | (chunk_y << 8) | chunk_z`
- Memory per chunk (uncompressed): 32³ × 1 byte = 32 KB

#### **Voxel Data Format**
```cpp
struct VoxelChunk {
  Vec3i min;              // world-space min corner
  uint8 voxels[32][32][32];
  
  bool dirty;             // mesh rebuild needed
  uint64 version;         // increment on modification
  uint64 lastModifiedTick;
  uint64 cachedMeshHash;  // for cache validation
};
```

**Voxel Type Encoding (1 byte):**
```
0x00 = empty (void)
0x01–0x7F = terrain types (rock, ice, crystal, metal, etc.)
0x80–0xFF = special (reserved for liquid, hazard, etc.)
```

#### **Compression (Persistence)**

**On-Disk (PostgreSQL BYTEA):**
- Sparse representation: only store non-empty voxels
- Run-length encoding: 
  - `(count: u16 | type: u8)` repeating
  - Reduces 32 KB chunk to ~2–8 KB on average (rocky terrain ≈ 75% empty)
- Alternative: Zstandard compression (if RLE insufficient)

```cpp
// Pseudocode: serialize chunk to bytes
vector<uint8> serialize_chunk(const VoxelChunk& chunk) {
  vector<uint8> result;
  uint8 last_type = chunk.voxels[0][0][0];
  uint32 count = 0;
  
  for (int x = 0; x < 32; ++x) {
    for (int y = 0; y < 32; ++y) {
      for (int z = 0; z < 32; ++z) {
        uint8 type = chunk.voxels[x][y][z];
        if (type == last_type && count < 65535) {
          ++count;
        } else {
          result.push_back((count >> 8) & 0xFF);
          result.push_back(count & 0xFF);
          result.push_back(last_type);
          last_type = type;
          count = 1;
        }
      }
    }
  }
  // Flush last run
  result.push_back((count >> 8) & 0xFF);
  result.push_back(count & 0xFF);
  result.push_back(last_type);
  return result;
}
```

### Mesh Generation

#### **Greedy Meshing Algorithm**

Goal: Reduce vertex count for rendering while preserving visual fidelity.

**Algorithm:**
1. For each axis (X, Y, Z):
   - Scan plane-by-plane perpendicular to axis
   - For each opaque voxel in plane:
     - Try to extend quad in 2D (greedy rectangle)
     - Stop at first empty or different-type voxel
     - Output quad (4 vertices + 2 triangles)
2. Result: ~30,000–50,000 triangles per chunk (vs. 520,000 if naive)

**Output Format (GPU-friendly):**
```cpp
struct MeshVertex {
  int16 x, y, z;        // (signed) local coords
  uint8 normal_packed;  // 3D normal packed into 1 byte (e.g., octahedron)
  uint8 voxel_type;     // terrain type (for texture lookup)
  uint8 ao;             // ambient occlusion (0–3)
};

struct ChunkMesh {
  vector<MeshVertex> vertices;
  vector<uint16> indices;
  AABB bounds;
};
```

**Caching:**
- Mesh regenerated only when chunk marked dirty
- GPU upload cached in VB/IB; CPU mesh evicted after upload
- Per-frame cost: ~0.5–1 ms for all visible chunks (async via GPU copy queue on modern DX12)

### Level of Detail (LOD)

| LOD | Distance | Mesh Reduction | Update Freq |
|-----|----------|-----------------|-----------|
| **LOD0** | 0–256 units (8 chunks) | 100% detail | 60 Hz server |
| **LOD1** | 256–512 units | ~60% vertices (every other voxel) | 20 Hz snapshot |
| **LOD2** | 512–1024 units | ~25% (4×4×4 voxels = 1 LOD2 voxel) | 10 Hz snapshot |
| **LOD3** | 1024+ units | Not rendered | — |

**Implementation (GPU):**
- Store all LOD meshes in same VB/IB
- Use different index ranges per LOD
- Pick LOD at draw time based on distance

**Implementation (CPU):**
- Generate LOD meshes offline or on-demand
- Store in chunk metadata
- Select before GPU upload

### Sector Streaming Strategy

**Streaming Pipeline:**
1. Client camera moves → new chunks enter view frustum
2. Client requests chunks via `CMD_REQUEST_CHUNK`
3. Server checks if chunk exists in voxel_chunks table
4. If exists: send chunk data in `SNAP_CHUNK` message
5. If not: send empty chunk (filled with terrain type 0x00)
6. Client receives chunk; unpacks voxel data → CPU RAM
7. On next frame: generate mesh via greedy meshing
8. Upload mesh to GPU (VB/IB); evict CPU voxel data if space needed

**Bandwidth Optimization:**
- Only send chunks once (sequence number tracking)
- When voxel delta received: update both server-side and client-side chunk
- If chunk modified mid-download: re-request (version mismatch detected)

### Voxel Destruction Mechanics

**Mining Example:**
1. Client sends `CMD_INPUT` with action=MINE, target voxel coords
2. Server validates: ship in range, voxel exists, voxel<br/>type is minable
3. Server removes voxel: `chunk.voxels[x][y][z] = 0x00`
4. Mark chunk dirty; append to voxel_events table
5. Server broadcasts `SNAP_STATE` with voxel delta to all players
6. Clients apply delta locally
7. When enough voxels removed: regenerate mesh (next frame)

**Persistence:**
- Every voxel removal written to voxel_events table
- Every 30 seconds (or on explicit flush): write full chunk snapshot to voxel_chunks
- On recovery: reconstruct final chunk state by replaying voxel_events since last snapshot

---

## Persistence Strategy

### Data Model

**Three-Tier Storage:**

1. **In-Memory Cache** (Server RAM)
   - Active chunks (< 1 minute old)
   - All entity objects
   - Voxel delta buffer
   - TTL: evict after 5 minutes inactivity

2. **Transactional Store** (PostgreSQL)
   - Player data (login, faction, resources)
   - Ship state (position, health, cargo)
   - Voxel chunk snapshots (compressed RLE)
   - Voxel event log (audit trail)

3. **Archive** (Optional S3 / backup)
   - Periodic snapshots for disaster recovery
   - Not required for MVP

### Detailed Schema

```sql
-- Sectors (metadata only)
CREATE TABLE sectors (
  sector_id VARCHAR(10) PRIMARY KEY,  -- e.g., "0,0"
  min_x INT, min_y INT, min_z INT,
  max_x INT, max_y INT, max_z INT,
  created_at TIMESTAMP DEFAULT NOW(),
  last_backup TIMESTAMP
);

-- Voxel chunks (materialized state)
CREATE TABLE voxel_chunks (
  chunk_id BYTEA PRIMARY KEY,
  sector_id VARCHAR(10) REFERENCES sectors(sector_id),
  voxel_data BYTEA,  -- RLE or zstd-compressed
  version INT DEFAULT 1,
  modified_at TIMESTAMP DEFAULT NOW(),
  data_hash VARCHAR(64)  -- for integrity check
);

-- Voxel change events (audit log)
CREATE TABLE voxel_events (
  event_id BIGSERIAL PRIMARY KEY,
  chunk_id BYTEA,
  world_x INT, world_y INT, world_z INT,
  old_voxel_type INT,
  new_voxel_type INT,
  player_id INT REFERENCES players(player_id),
  event_time TIMESTAMP DEFAULT NOW(),
  tick_number BIGINT
);

CREATE INDEX idx_voxel_events_chunk ON voxel_events(chunk_id, event_time DESC);
CREATE INDEX idx_voxel_events_time ON voxel_events(event_time DESC);
CREATE INDEX idx_voxel_chunks_sector ON voxel_chunks(sector_id);

-- Players
CREATE TABLE players (
  player_id INT PRIMARY KEY,
  username VARCHAR(64) UNIQUE NOT NULL,
  password_hash VARCHAR(256),
  faction INT DEFAULT 0,
  credits INT DEFAULT 1000,
  created_at TIMESTAMP DEFAULT NOW(),
  last_login TIMESTAMP
);

-- Ships (owned by players)
CREATE TABLE ships (
  ship_id INT PRIMARY KEY,
  player_id INT NOT NULL REFERENCES players(player_id) ON DELETE CASCADE,
  ship_name VARCHAR(64),
  ship_type INT,  -- 0=fighter, 1=corvette, 2=carrier, etc.
  sector_id VARCHAR(10),
  pos_x REAL, pos_y REAL, pos_z REAL,
  rot_yaw REAL, rot_pitch REAL, rot_roll REAL,
  vel_x REAL, vel_y REAL, vel_z REAL,
  hp INT, max_hp INT,
  shield_hp INT, max_shield_hp INT,
  cargo_resource INT,
  cargo_capacity INT,
  updated_at TIMESTAMP DEFAULT NOW(),
  
  FOREIGN KEY (player_id) REFERENCES players(player_id)
);

CREATE INDEX idx_ships_player ON ships(player_id);
CREATE INDEX idx_ships_sector ON ships(sector_id);

-- Fleets (optional: group ships)
CREATE TABLE fleets (
  fleet_id INT PRIMARY KEY,
  player_id INT NOT NULL REFERENCES players(player_id) ON DELETE CASCADE,
  fleet_name VARCHAR(64),
  created_at TIMESTAMP DEFAULT NOW()
);

CREATE TABLE fleet_membership (
  fleet_id INT REFERENCES fleets(fleet_id) ON DELETE CASCADE,
  ship_id INT REFERENCES ships(ship_id) ON DELETE CASCADE,
  PRIMARY KEY (fleet_id, ship_id)
);

-- Session state (transient; can be lost on restart)
CREATE TABLE sessions (
  session_id VARCHAR(128) PRIMARY KEY,
  player_id INT REFERENCES players(player_id),
  login_time TIMESTAMP DEFAULT NOW(),
  last_heartbeat TIMESTAMP DEFAULT NOW(),
  ip_address INET,
  
  CONSTRAINT unique_player_session UNIQUE (player_id)
);
```

### Write & Flush Strategy

**Per-Tick Operations:**
```
For each command:
  - Apply to entity (ship)
  - Mark entity "modified"

At end of tick (60 Hz):
  - Buffer voxel events to in-memory deltaBuffer
  - Mark chunks "dirty" if voxels changed

Every 1 second (or 60 ticks):
  - Flush voxel_events table (batch insert all buffered deltas)
  - For each dirty chunk: write to voxel_chunks (INSERT or UPDATE)

On demand (player logout, emergency):
  - Flush all ship state to database
  - Flush all voxel deltas
```

**Example:** 
```cpp
// Per-tick voxel destruction
void ApplyVoxelDestruction(Voxel target_voxel) {
  VoxelChunk& chunk = GetChunk(target_voxel);
  uint8 old_type = chunk.voxels[target_voxel.local_x][...][...];
  chunk.voxels[...][...][...] = 0x00;
  
  VoxelDelta delta;
  delta.world_x = target_voxel.world_x;
  delta.world_y = target_voxel.world_y;
  delta.world_z = target_voxel.world_z;
  delta.old_voxel_type = old_type;
  delta.new_voxel_type = 0x00;
  delta.player_id = attacker_player_id;
  delta.tick = g_current_tick;
  
  g_voxel_delta_buffer.push_back(delta);
  chunk.dirty = true;
  chunk.version++;
}

// Flush every 1 second (60 ticks @60Hz)
void FlushVoxelDeltas() {
  if (g_voxel_delta_buffer.empty()) return;
  
  // Batch SQL INSERT
  stringstream sql;
  sql << "INSERT INTO voxel_events (chunk_id, world_x, world_y, world_z, "
      << "old_voxel_type, new_voxel_type, player_id, tick_number) VALUES ";
  
  for (const auto& delta : g_voxel_delta_buffer) {
    sql << "(" << delta.chunk_id << ", " << delta.world_x << ", ...," << delta.new_voxel_type << "), ";
  }
  // Remove trailing comma and execute
  
  g_database.Execute(sql.str());
  g_voxel_delta_buffer.clear();
}

// Flush dirty chunks every 30 seconds
void FlushDirtyChunks() {
  for (auto& [chunk_id, chunk] : g_voxel_chunks) {
    if (!chunk.dirty) continue;
    
    vector<uint8> compressed = SerializeChunk(chunk);
    uint64 hash = CRC64(compressed);
    
    g_database.Execute(
      "INSERT INTO voxel_chunks (chunk_id, sector_id, voxel_data, version, data_hash) "
      "VALUES (?, ?, ?, ?, ?) "
      "ON CONFLICT (chunk_id) DO UPDATE SET "
      "voxel_data = EXCLUDED.voxel_data, version = version + 1, modified_at = NOW()",
      chunk_id, chunk.sector_id, compressed, chunk.version, hash
    );
    
    chunk.dirty = false;
  }
}
```

### Recovery & Consistency

**On Server Start:**
```
1. Load all voxel_chunks from DB into memory
2. Load all active players/ships
3. Reconstruct voxel delta buffer from voxel_events 
   (from last server_shutdown_tick onward)
4. For each dirty chunk at shutdown time:
   Replay deltas to reconstruct final state
5. Start simulation loop
```

**Data Consistency:**
- Voxel events use `tick_number` to preserve causality
- Ship position uses `updated_at` timestamp for conflict resolution
- Player logout: save ship and resources immediately (transactional)

---

## Rendering Pipeline

### Achieving the Retro Pixel Voxel Look

**Visual Goals:**
- Low-poly angular silhouettes
- Pixelated appearance (pixel-art texture)
- Glowing neon fissures
- Dark rocky terrain with edge highlights
- Minimal lighting complexity

### Rendering Process

```mermaid
graph LR
    VoxelGeom["Voxel Geometry<br/>(per-chunk mesh)"]
    EntityGeom["Entity Geometry<br/>(ships, FX)"]
    
    VoxelGeom -->|"Render to<br/>LDR RT"| ColorRT["Color RT<br/>(640×360)"]
    EntityGeom -->|"Render to<br/>same RT"| ColorRT
    
    ColorRT -->|"Apply bloom<br/>on emissive"| BloomRT["Bloom RT<br/>(320×180)"]
    
    ColorRT -->|"Box blur +<br/>tone map"| ToneRT["Tone-mapped<br/>(640×360)"]
    
    BloomRT -->|"Blur 4 levels<br/>with weights"| GlowPyramid["Glow Pyramid"]
    
    ToneRT -->|"Composite<br/>with glow"| CompositeRT["Composite RT"]
    
    CompositeRT -->|"Upsample nearest-neighbor<br/>to native resolution"| Backbuffer["Backbuffer<br/>(1920×1080)"]
    
    Backbuffer -->|"Present to<br/>swap chain"| Monitor["Monitor"]
    
    style VoxelGeom fill:#0066cc,stroke:#fff,color:#fff
    style EntityGeom fill:#0066cc,stroke:#fff,color:#fff
    style ColorRT fill:#ff9900,stroke:#000
    style BloomRT fill:#ffff00,stroke:#000
    style ToneRT fill:#ff9900,stroke:#000
    style GlowPyramid fill:#ffff00,stroke:#000
    style Backbuffer fill:#00cc00,stroke:#fff,color:#fff
```

### Stage 1: Voxel Geometry Rendering

**Vertex Shader**
```hlsl
// Input: voxel mesh vertex (32-bit packed)
cbuffer SceneConstants : register(b0) {
  float4x4 view;
  float4x4 projection;
  float3 camera_pos;
  float padding;
};

struct VS_INPUT {
  int3 pos : POSITION;        // local voxel coords (-16 to +16)
  uint normal_packed : NORMAL; // octahedron-encoded normal
  uint voxel_type : TEXCOORD0;
  uint ao : TEXCOORD1;         // ambient occlusion 0–3
};

struct VS_OUTPUT {
  float4 pos_clip : SV_POSITION;
  float3 normal_world : NORMAL;
  float3 pos_world : TEXCOORD0;
  uint voxel_type : TEXCOORD1;
  uint ao : TEXCOORD2;
};

VS_OUTPUT main(VS_INPUT input) {
  VS_OUTPUT output;
  
  // Unpack normal from octahedron
  float3 normal_local = UnpackNormal(input.normal_packed);
  
  // Transform to world space (chunk offset + local pos)
  float3 pos_world = chunk_min + float3(input.pos);
  float3 normal_world = normalize(mul(normal_local, (float3x3)world));
  
  // Project to screen
  float4 pos_view = mul(float4(pos_world, 1), view);
  output.pos_clip = mul(pos_view, projection);
  
  output.pos_world = pos_world;
  output.voxel_type = input.voxel_type;
  output.ao = input.ao;
  output.normal_world = normal_world;
  
  return output;
}
```

**Pixel Shader**
```hlsl
sampler2D tex_terrain : register(s0);        // diffuse atlas
sampler2D tex_emissive_mask : register(s1);  // emissive layer mask

float4 main(VS_OUTPUT input) : SV_TARGET {
  // Fetch terrain color from atlas (voxel_type determines UV offset)
  float2 uv_terrain = GetTerrainUV(input.voxel_type, 0.5, 0.5);
  float3 color_base = tex_terrain.Sample(sampler_terrain, uv_terrain).rgb;
  
  // Check if voxel type is emissive (energy fissure)
  float is_emissive = tex_emissive_mask.Sample(sampler, float2(input.voxel_type / 256.0, 0.5)).r;
  
  // Simple lighting: face-normal dot light direction
  float3 light_dir = normalize(float3(0.7, 0.7, 0.5)); // sun direction
  float diffuse = max(0.2, dot(input.normal_world, light_dir)); // 0.2 = ambient
  
  // Ambient occlusion from voxel corner data
  float ao_factor = 1.0 - (input.ao * 0.1); // 0–3 -> 1.0–0.7
  diffuse *= ao_factor;
  
  // Apply diffuse
  float3 color_lit = color_base * diffuse;
  
  // Add emissive glow (neon fissures)
  float3 emissive = color_lit * is_emissive * 2.0; // double brightness for glow
  
  // Output to LDR RT (will be tone-mapped later)
  return float4(color_lit + emissive, 1.0);
}
```

### Stage 2: Entity Rendering

**Ships & Projectiles**
- Render with simple diffuse + emissive (shield glow, engine thrust)
- Billboard particles for explosions/mining sparks
- Layer above voxel geometry (draw order)

### Stage 3: Post-Processing

#### **3a. Bloom Extraction**

```hlsl
// Extract bright pixels for bloom
float4 bloom_ps(float2 uv : TEXCOORD0) : SV_TARGET {
  float4 color = tex_color.Sample(sampler, uv);
  float brightness = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
  
  // Only pixels brighter than 1.0 contribute to bloom
  float is_bright = step(1.0, brightness);
  return float4(color.rgb * is_bright, 1.0);
}
```

#### **3b. Glow Pyramid (Downsampled Blur)**

```
Bloom RT (640×360) 
  → Level 1: 320×180 (4-tap box blur)
  → Level 2: 160×90  (4-tap)
  → Level 3: 80×45   (4-tap)
  → Level 4: 40×22   (4-tap)

Composite: Upsample L4 → L3 → L2 → L1, add to original
```

Result: Soft glowing halo around bright areas (neon fissures, explosions).

#### **3c. Pixelation (Nearest-Neighbor Upsample)**

```hlsl
// Downsample to ~8x8 pixel blocks, then upsample with nearest-neighbor
float4 pixelate_ps(float2 uv : TEXCOORD0) : SV_TARGET {
  // uv is in screen space (0–1)
  
  // Quantize to nearest pixel block (e.g., 8x8 for 1920×1080 -> 240×135)
  float2 pixel_size = 1.0 / render_target_dims; // e.g., 1/640, 1/360
  float block_size = 8.0;  // pixels per block
  
  float2 block_uv = floor(uv / (block_size * pixel_size)) * block_size * pixel_size;
  
  // Sample center of block
  return tex_color.Sample(sampler, block_uv + block_size * pixel_size * 0.5);
}
```

Actually for MVP, simpler approach:
1. Render all geometry to 640×360 render target (low res)
2. Apply glow/bloom at 640×360
3. Nearest-neighbor upsample to 1920×1080
4. Present

DirectX 12 supports `D3D12_FILTER_MIN_MAG_MIP_POINT` for nearest-neighbor in samplers.

### Stage 4: Composite & Present

```hlsl
// Composite tone-mapped scene + bloom glow
float4 composite_ps(float2 uv : TEXCOORD0) : SV_TARGET {
  float4 scene_tone = tex_scene.Sample(sampler, uv);
  float4 glow = tex_bloom.Sample(sampler, uv);
  
  // Additive blend glow onto scene
  float3 result = scene_tone.rgb + glow.rgb * 0.5; // glow contribution
  
  // Clamp to LDR (8-bit display)
  result = saturate(result);
  
  return float4(result, 1.0);
}
```

### Shader Code Organization

```
/client/shaders/
  voxel.vs                 -- voxel vertex shader
  voxel.ps                 -- voxel pixel shader
  entity.vs                -- ship/projectile vertex
  entity.ps                -- ship/projectile pixel
  bloom_extract.ps         -- bloom mask
  blur_h.ps, blur_v.ps     -- directional blur (glow)
  composite.ps             -- final composite
  pixelate_vs, pixelate_ps -- upsample stage
  
shaders.hlsl               -- shared utilities
  UnpackNormal()
  GetTerrainUV()
```

### DirectX 12 Integration

**PSO (Pipeline State Objects):**

```cpp
D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_voxel_world = {
  .pRootSignature = root_sig_voxel,
  .VS = { shader_voxel_vs.data(), shader_voxel_vs.size() },
  .PS = { shader_voxel_ps.data(), shader_voxel_ps.size() },
  .BlendState = { .RenderTarget[0] = {
    .BlendEnable = FALSE,
    .LogicOpEnable = FALSE,
  } },
  .RasterizerState = {
    .FillMode = D3D12_FILL_MODE_SOLID,
    .CullMode = D3D12_CULL_MODE_BACK,
    .FrontCounterClockwise = TRUE,
  },
  .DepthStencilState = {
    .DepthEnable = TRUE,
    .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
    .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
  },
  .InputLayout = { &input_layout, 1 },
  .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
  .NumRenderTargets = 1,
  .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
  .DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
  .SampleDesc = { .Count = 1, .Quality = 0 },
};
device->CreateGraphicsPipelineState(&pso_voxel_world, IID_PPV_ARGS(&pso_voxel_world_obj));
```

**Descriptor Heaps:**
- One per-frame descriptor heap for CBV/SRV/UAV
- Allocate space for up to 200 voxel chunks × 1 SRV each
- Shader visible heap for GPU access

**Command List Recording (per-frame):**

```cpp
void RenderVoxelWorld(ID3D12GraphicsCommandList* cmd_list, float delta_time) {
  // Bind voxel world PSO
  cmd_list->SetPipelineState(pso_voxel_world_obj);
  cmd_list->SetGraphicsRootSignature(root_sig_voxel);
  
  // Bind scene constants (view, projection, camera pos)
  cmd_list->SetGraphicsRootConstantBufferView(0, cbuffer_scene_gpu_addr);
  
  // Render each visible chunk
  for (const auto& chunk : visible_chunks) {
    if (!chunk.mesh_valid) continue; // mesh not ready
    
    // Bind chunk VB/IB
    D3D12_VERTEX_BUFFER_VIEW vbv = { 
      chunk.mesh_vb->GetGPUVirtualAddress(),
      chunk.mesh.vertices.size() * sizeof(MeshVertex),
      sizeof(MeshVertex)
    };
    cmd_list->IASetVertexBuffers(0, 1, &vbv);
    
    D3D12_INDEX_BUFFER_VIEW ibv = {
      chunk.mesh_ib->GetGPUVirtualAddress(),
      chunk.mesh.indices.size() * sizeof(uint16),
      DXGI_FORMAT_R16_UINT
    };
    cmd_list->IASetIndexBuffer(&ibv);
    
    // Bind chunk SRV (diffuse + emissive textures)
    D3D12_GPU_DESCRIPTOR_HANDLE srv_handle = { descriptor_heap_gpu.ptr + chunk.descriptor_offset };
    cmd_list->SetGraphicsRootDescriptorTable(1, srv_handle);
    
    // Draw
    cmd_list->DrawIndexedInstanced(chunk.mesh.indices.size(), 1, 0, 0, 0);
  }
}
```

---

## Deployment Architecture

### Docker Container Layout

```dockerfile
# Dockerfile (Windows Server Core)
FROM mcr.microsoft.com/windows/servercore:ltsc2022 AS builder

# Install Visual Studio Build Tools + CMake + vcpkg deps
RUN powershell -Command \
    Invoke-WebRequest -Uri https://aka.ms/vs/17/release/vs_buildtools.exe -OutFile vs_buildtools.exe ; \
    Start-Process -Wait -FilePath .\vs_buildtools.exe -ArgumentList '--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended' ; \
    Remove-Item vs_buildtools.exe

WORKDIR C:\app
COPY . .

RUN cmake --preset x64-release && cmake --build out/build/x64-release

###

FROM mcr.microsoft.com/windows/servercore:ltsc2022

WORKDIR C:\app

COPY --from=builder C:\app\out\build\x64-release\Server\Server.exe C:\app\server.exe
COPY config\ C:\app\config\

ENV LOG_LEVEL=INFO
ENV DATABASE_URL=postgres://user:pass@postgres:5432/starstrike

EXPOSE 7777/udp

HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=3 \
  CMD powershell -Command "C:\app\server.exe --health-check; if ($LASTEXITCODE -ne 0) { exit 1 }"

ENTRYPOINT ["C:\\app\\server.exe"]
CMD ["--config", "C:\\app\\config\\server.yaml"]
```

### Deployment Diagram

```mermaid
graph TB
    Nginx["Nginx Reverse Proxy<br/>Port 443 (HTTPS)"]
    Docker["Docker Container<br/>StarStrike Server"]
    Postgres["PostgreSQL 14<br/>Persistent Storage"]
    Prometheus["Prometheus<br/>Metrics Scrape"]
    Grafana["Grafana<br/>Dashboard"]
    
    User["Players<br/>UDP:7777"]
    Logs["Log Volume<br/>C:\\logs\\starstrike"]
    Data["Data Volume<br/>/var/lib/postgres"]
    Config["Config Volume<br/>C:\\app\\config"]
    
    User -->|"UDP Port 7777"| Docker
    Docker -->|"Query/Update/Insert"| Postgres
    Docker -->|"Metrics at :9090"| Prometheus
    Prometheus -->|"Scrape"| Grafana
    
    Config -->|"Mounts to"| Docker
    Logs -->|"Mounts to"| Docker
    Data -->|"Mounts to"| Postgres
    
    style Docker fill:#0066cc,stroke:#fff,color:#fff
    style Postgres fill:#0a8c42,stroke:#fff,color:#fff
    style Prometheus fill:#ff9900,stroke:#000
```

### docker-compose.yaml (Development)

```yaml
version: '3.9'

services:
  postgres:
    image: postgres:14
    environment:
      POSTGRES_USER: starstrike
      POSTGRES_PASSWORD: dev_password_change_in_prod
      POSTGRES_DB: starstrike
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./schema.sql:/docker-entrypoint-initdb.d/01-init.sql
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U starstrike"]
      interval: 10s
      timeout: 5s
      retries: 5

  server:
    build: 
      context: .
      dockerfile: Dockerfile
    environment:
      LOG_LEVEL: DEBUG
      DATABASE_URL: postgres://starstrike:dev_password_change_in_prod@postgres:5432/starstrike
      SERVER_TICK_RATE: 60
      MAX_PLAYERS: 50
    ports:
      - "7777:7777/udp"
      - "9090:9090"  # Prometheus metrics
    depends_on:
      postgres:
        condition: service_healthy
    volumes:
      - ./config:C:\app\config:ro
      - server_logs:C:\logs\starstrike
    restart: unless-stopped

volumes:
  postgres_data:
  server_logs:
```

### Configuration File (YAML)

```yaml
# config/server.yaml

server:
  port: 7777
  bind_address: "0.0.0.0"
  tick_rate_hz: 60
  max_concurrent_players: 50
  
database:
  url: "${DATABASE_URL}"
  max_pool_size: 20
  connection_timeout_ms: 5000
  query_timeout_ms: 30000
  
world:
  sectors:
    grid_x: 4
    grid_y: 4
    sector_size_voxels: 512
  
  chunk:
    size: 32  # voxels per axis
    max_memory_mb: 2048
    dirty_flush_interval_sec: 30
  
  voxel:
    streaming_radius_chunks: 8
    lod_distances: [8, 16, 32]
  
network:
  bandwidth_limit_kbps: 1000  # per-player
  snapshot_rate_hz: 20
  packet_loss_tolerance_pct: 5
  
logging:
  level: "${LOG_LEVEL:INFO}"
  file: "/var/log/starstrike/server.log"
  max_size_mb: 100
  max_backups: 10
  
metrics:
  enabled: true
  port: 9090
  scrape_interval_sec: 10
```

### Restart & Lifecycle

**Graceful Shutdown:**
```cpp
void OnShutdownSignal(int sig) {
  g_shutdown_requested = true;
  
  // Server loop checks g_shutdown_requested each iteration
  // Flushes voxels, saves player state, closes connections
  // Exits cleanly within 5 seconds
}

signal(SIGTERM, OnShutdownSignal);
signal(SIGINT, OnShutdownSignal);
```

**Kubernetes Ready (Future):**
```yaml
livenessProbe:
  httpGet:
    path: /health
    port: 9090
  initialDelaySeconds: 30
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /ready
    port: 9090
  initialDelaySeconds: 60
  periodSeconds: 5
```

---

## Observability

### Logging

**Levels & Categories:**

```cpp
enum class LogLevel {
  TRACE,    // every voxel update
  DEBUG,    // per-tick state changes
  INFO,     // startup, player join/leave
  WARN,     // bandwidth overages, late ticks
  ERROR,    // game logic violations
  FATAL     // shutdown-triggering errors
};

#define LOG(level, category, format, ...) \
  Logger::Write(level, category, __FILE__, __LINE__, format, ##__VA_ARGS__)

// Examples:
LOG(INFO, "NETWORK", "Player %d connected from %s", player_id, ip_addr);
LOG(DEBUG, "VOXEL", "Chunk [%d, %d, %d] modified, triggering mesh rebuild", cx, cy, cz);
LOG(WARN, "TICK", "Tick %lld took %.2f ms (budget: 16.67 ms)", tick, tick_duration_ms);
```

**Output Targets:**
- Stdout (container logs)
- File rotation (100 MB max, 10 backups)
- Optional: Syslog or centralized ELK stack (future)

### Metrics (Prometheus)

**Key Metrics:**

```
# Server heartbeat
server_tick_count_total (counter)
server_tick_duration_ms (histogram: min/max/p50/p95/p99)
server_uptime_seconds (gauge)

# Network
network_packets_received_total (counter, by type)
network_packets_sent_total (counter)
network_connections_active (gauge)
network_bandwidth_kbps (gauge, by direction)

# Gameplay
entities_count (gauge: ships, asteroids, projectiles, effects)
players_connected (gauge)
players_logged_in_total (counter)

# Voxel system
voxel_chunks_loaded (gauge)
voxel_chunks_dirty (gauge)
voxel_destruction_events_total (counter)
voxel_mesh_rebuild_ms (histogram)

# Database
db_query_duration_ms (histogram, by operation type)
db_connection_pool_size (gauge)
db_errors_total (counter)

# Combat & gameplay
combat_hits_total (counter)
mining_voxels_extracted (counter)
resource_mined_total (counter)
```

**Prometheus Scrape Config:**
```yaml
global:
  scrape_interval: 10s

scrape_configs:
  - job_name: 'starstrike-server'
    static_configs:
      - targets: ['localhost:9090']
```

**Grafana Dashboard Panels:**
- Tick timing (graph: duration over time)
- Active players (gauge)
- Network throughput (stacked area: inbound vs. outbound)
- Entity count (stacked bar: ships, asteroids, projectiles)
- Voxel chunks (gauge: loaded, dirty, evicted)
- Database latency (histogram: p50, p95, p99)

### Server Health Check

```cpp
struct ServerHealth {
  bool is_running;
  uint64 current_tick;
  uint64 last_tick_time_ms;
  int active_players;
  int active_chunks;
  float tick_time_ms;
  float bandwidth_kbps;
  
  bool IsHealthy() const {
    return is_running 
      && tick_time_ms < 20.0f  // budget is 16.67, allow 20% overrun
      && active_players > 0 || last_tick_time_ms < 60000; // no tick for 60 sec = bad
  }
};

// Exposed via HTTP endpoint :9090/health
void HandleHealthCheck(HttpRequest req) {
  ServerHealth health = GetServerHealth();
  Json::Value json;
  json["status"] = health.IsHealthy() ? "healthy" : "degraded";
  json["tick"] = health.current_tick;
  json["players"] = health.active_players;
  json["tick_time_ms"] = health.tick_time_ms;
  req.SendJson(json);
}
```

---

## Repository Layout

### Mono-Repo Structure

```
StarStrike.RTS/
│
├── README.md                      # Project overview
├── CMakeLists.txt                # Root CMake (orchestrates builds)
├── CMakePresets.json             # Build presets (debug, release, etc.)
├── Dockerfile                    # Windows Server Core container definition
├── docker-compose.yaml           # Local dev compose
│
├── client/                       # Client application (Windows, DirectX 12)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp              # Entry point, window creation
│   │   ├── app.cpp/h             # Game app class
│   │   ├── rendering/
│   │   │   ├── dx12_device.cpp/h    # DX12 device init & management
│   │   │   ├── voxel_renderer.cpp/h # Voxel mesh generation & draw
│   │   │   ├── entity_renderer.cpp/h # Ships, projectiles
│   │   │   ├── ui_renderer.cpp/h    # HUD, menus
│   │   │   └── post_process.cpp/h   # Bloom, pixelation
│   │   ├── world/
│   │   │   ├── camera.cpp/h         # Isometric camera controller
│   │   │   ├── voxel_cache.cpp/h    # Chunk streaming & LOD
│   │   │   ├── entity_list.cpp/h    # Local entity AoS storage
│   │   │   └── chunk_streamer.cpp/h # Interest mgmt
│   │   ├── network/
│   │   │   ├── socket.cpp/h         # UDP socket
│   │   │   ├── packet_codec.cpp/h   # Serialize/deserialize
│   │   │   ├── snapshot_decoder.cpp/h
│   │   │   └── packet_queue.cpp/h   # Input queue
│   │   ├── input/
│   │   │   ├── keyboard.cpp/h       # Win32 keyboard input
│   │   │   ├── mouse.cpp/h          # Mouse + camera control
│   │   │   └── command_input.cpp/h  # Command translation
│   │   ├── simulation/
│   │   │   ├── ship_predictor.cpp/h # Dead reckoning
│   │   │   └── projectile_predictor.cpp/h
│   │   └── assets/
│   │       ├── shader_compiler.cpp/h # HLSL → bytecode
│   │       ├── mesh_pool.cpp/h       # VB/IB allocation
│   │       └── texture_manager.cpp/h
│   │
│   └── shaders/
│       ├── voxel.hlsl               # Voxel vertex + pixel
│       ├── entity.hlsl              # Entity rendering
│       ├── bloom_extract.hlsl       # Bloom mask
│       ├── blur.hlsl                # Glow blur
│       ├── composite.hlsl           # Final composite
│       └── shared.hlsl              # Utility functions
│
├── server/                       # Server application (C++, UDP, PostgreSQL)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp              # Entry point, signal handlers
│   │   ├── server.cpp/h          # Server main loop
│   │   ├── config.cpp/h          # YAML config loader
│   │   ├── transport/
│   │   │   ├── socket.cpp/h         # UDP socket manager
│   │   │   ├── packet_decoder.cpp/h # Deserialize input
│   │   │   ├── packet_encoder.cpp/h # Serialize snapshots
│   │   │   └── rate_limiter.cpp/h   # Per-player throttle
│   │   ├── simulation/
│   │   │   ├── engine.cpp/h         # 60 Hz tick loop
│   │   │   ├── command_processor.cpp/h
│   │   │   ├── physics.cpp/h        # AABB, raycasts
│   │   │   └── combat_system.cpp/h  # Damage, effects
│   │   ├── world/
│   │   │   ├── sector.cpp/h         # Sector manager
│   │   │   ├── entity_system.cpp/h  # ECS-lite
│   │   │   ├── voxel_system.cpp/h   # Chunk + delta mgmt
│   │   │   ├── voxel_serialization.cpp/h
│   │   │   └── mining_system.cpp/h  # Resource extraction
│   │   ├── persistence/
│   │   │   ├── database.cpp/h       # PostgreSQL connection pool
│   │   │   ├── chunk_store.cpp/h    # Voxel chunk CRUD
│   │   │   ├── entity_store.cpp/h   # Ship/player CRUD
│   │   │   └── transaction_log.cpp/h # Event log
│   │   ├── interest_mgmt/
│   │   │   ├── visibility.cpp/h     # Distance culling
│   │   │   ├── snapshot_builder.cpp/h
│   │   │   └── chunk_request_mgr.cpp/h
│   │   └── observability/
│   │       ├── logger.cpp/h         # Logging
│   │       ├── metrics.cpp/h        # Prometheus metrics
│   │       └── health.cpp/h         # Health check endpoint
│   │
│   └── cmake/
│       └── FindPostgreSQL.cmake    # PostgreSQL discovery
│
├── shared/                       # Shared code (types, serialization)
│   ├── CMakeLists.txt
│   ├── types.h                   # EntityID, Vec3, Quat, etc.
│   ├── constants.h               # Chunk sizes, physics params, etc.
│   ├── entity_types.h            # Ship, Asteroid enum + properties
│   ├── packet_types.h            # Network message structs
│   ├── serialization.h           # Binary encode/decode utilities
│   └── math.h                    # Common math functions
│
├── tools/                        # Utilities & converters
│   ├── CMakeLists.txt
│   ├── asset_compiler/           # Texture atlas builder
│   │   ├── atlas_builder.cpp
│   │   └── png_loader.cpp
│   └── voxel_painter/            # Level editor (future)
│       ├── editor.cpp
│       └── voxel_brush.cpp
│
├── assets/                       # Game content
│   ├── textures/
│   │   ├── terrain_atlas.png     # 2048×2048 RGBA
│   │   ├── emissive_mask.png
│   │   └── normal_maps.png
│   ├── scripts/
│   │   ├── terrain_gen.lua       # Procedural generation
│   │   └── ai_behavior.lua
│   └── data/
│       ├── ship_types.yaml       # Ship stats
│       ├── weapons.yaml          # Weapon properties
│       └── factions.yaml
│
├── config/                       # Deployment configs
│   ├── server.yaml               # Server configuration
│   ├── schema.sql                # PostgreSQL schema
│   └── prometheus.yml            # Prometheus config
│
├── docs/                         # Documentation
│   ├── ARCHITECTURE.md           # This file
│   ├── NETWORKING.md             # Protocol spec
│   ├── VOXEL_SYSTEM.md           # Voxel design detailed
│   ├── RENDERING.md              # Renderer internals
│   └── BUILD.md                  # Build instructions
│
└── .github/                      # CI/CD
    └── workflows/
        ├── build.yml             # Compile on every commit
        ├── test.yml              # Unit tests
        └── deploy.yml            # Docker build & push
```

### Build System (CMake)

**Root CMakeLists.txt:**

```cmake
cmake_minimum_required(VERSION 3.20)
project(StarStrike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
if (MSVC)
  add_compile_options(/W4 /WX)  # Warnings as errors
else ()
  add_compile_options(-Wall -Wextra -Werror)
endif ()

# Find dependencies
find_package(PostgreSQL REQUIRED)
find_package(DirectX REQUIRED)

# Core libraries (see ARCHITECTURE_RECOMMENDATIONS.md for full CMake structure)
add_subdirectory(shared)  # Creates NeuronCore, Neuron, GameLogic

# Server
add_subdirectory(server)

# Client (Windows only)
if (WIN32)
  add_subdirectory(client)
endif ()

# Tools
add_subdirectory(tools)
```

**server/CMakeLists.txt:**

```cmake
add_executable(NeuronServer
  src/main.cpp
  src/server.cpp
  src/config.cpp
  # ... rest of sources
)

target_link_libraries(NeuronServer PRIVATE
  GameLogic
  Neuron
  NeuronCore
  PostgreSQL::PostgreSQL
)

target_compile_definitions(NeuronServer PRIVATE
  NEURON_VERSION="0.1.0"
  NEURON_MAX_PLAYERS=50
)
```

### CI/CD (.github/workflows/build.yml)

```yaml
name: Build & Test

on: [push, pull_request]

jobs:
  build-server:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake postgresql-client libpq-dev
      
      - name: Build server
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build
      
      - name: Build Docker image
        run: docker build -t starstrike:latest .
  
  build-client:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Setup MSVC
        uses: ilammy/msvc-dev-cmd@v1
      
      - name: Build client
        run: |
          cmake -B build -G "Visual Studio 17 2022" -A x64
          cmake --build build --config Release
```

---

## MVP Plan

### 8–12 Week Breakdown

**Week 1: Foundation & Network**
- Deliverables:
  - Basic server runs; tick loop working
  - UDP socket listens on port 7777
  - Client connects; handshake exchange
  - Packet codec (serialization) for basic commands
- Success Criteria:
  - Server prints "Tick N" every 16.7 ms
  - Client sends & receives ping

**Week 2: Voxel Loading & Persistence**
- Deliverables:
  - PostgreSQL schema created
  - Voxel chunk CRUD in server
  - Chunk serialization (RLE compression)
  - Client fetches chunk; renders single chunk with debug coloring
- Success Criteria:
  - One sector (4 chunks) loads from DB on server start
  - Client displays chunk with distinct colors per voxel type

**Week 3: Voxel Rendering Pipeline**
- Deliverables:
  - DirectX 12 device init (client)
  - Greedy mesh generation for voxels
  - Voxel renderer (vertex + pixel shaders)
  - Basic lighting (diffuse only)
- Success Criteria:
  - Client renders low-poly asteroids with proper shading
  - Framerate > 60 FPS with 200 visible chunks

**Week 4: Entity System & Ship Movement**
- Deliverables:
  - Entity struct + AoS storage (server)
  - Spawn ships at start
  - Basic movement command (Move to point A)
  - Client prediction for smooth movement
  - Network snapshot sending (20 Hz)
- Success Criteria:
  - 2 players can see each other's ships
  - Ships move smoothly without jittering
  - Bandwidth < 50 KB/s per player

**Week 5: Input & Camera**
- Deliverables:
  - Keyboard/mouse input (client)
  - Isometric camera controller (pan, zoom)
  - HUD: health bar, ammo counter, radar (basic)
  - Command input mapping
- Success Criteria:
  - Player can control own ship with WASD
  - Camera smooth; zoom works

**Week 6: Combat System**
- Deliverables:
  - Weapon firing command
  - Projectile entity type
  - Hit detection (raycasts)
  - Damage application
  - Visual feedback (explosion particle FX)
- Success Criteria:
  - 2 players can shoot at each other
  - Hits register; ships take damage; despawn at 0 HP

**Week 7: Mining & Resource System**
- Deliverables:
  - Mining beam command
  - Voxel destruction on server
  - Delta replication to all clients
  - Resource item spawn + collection
  - Cargo UI display
- Success Criteria:
  - Player mines asteroid; voxel removed for all
  - Resources appear; can be picked up
  - Cargo counter updates

**Week 8: Persistence & Save/Load**
- Deliverables:
  - Player table in DB
  - Ship state persistence (position, HP, cargo)
  - Voxel delta log
  - Player login: load ships from DB
  - Graceful shutdown: flush all state
- Success Criteria:
  - Server restart: world state unchanged
  - Player A logs out, ships persist
  - 50 mining voxels written to event log per second without lag

**Week 9: UI & Menus**
- Deliverables:
  - Main menu (single-player or connect to server)
  - Chat HUD (see other players' messages)
  - Settings menu
  - Pause menu

**Week 10: Multi-Player Stability**
- Deliverables:
  - Stress testing with 50 concurrent players
  - Network error handling (packet loss, latency)
  - Server-side rate limiting
  - Observability (logging, metrics, health check)
- Success Criteria:
  - 50 players connected; no crashes
  - Tick time < 16.67 ms @ 100% CPU utilization
  - Bandwidth capped per player

**Week 11: Post-Processing & Visual Polish**
- Deliverables:
  - Bloom extraction & glow
  - Pixelation post-process
  - Emissive voxel types (neon fissures)
  - Shield/engine glow on ships
- Success Criteria:
  - Neon fissures glow; retro pixel aesthetic achieved

**Week 12: Testing & Shipping**
- Deliverables:
  - Bug fixes from playtesting
  - Performance profiling
  - Docker container tested
  - Deployment guide written
  - Documentation complete

### Release Checklist (MVP)

**Functional:**
- [ ] 50 players can connect simultaneously
- [ ] Ships move, take damage, die
- [ ] Mining removes voxels permanently (all see change)
- [ ] Weapons fire and hit correctly
- [ ] Resources collected and stored
- [ ] Chat works

**Technical:**
- [ ] Server tick runs at 60 Hz, < 16.67 ms per tick
- [ ] Client 60 FPS with voxel rendering
- [ ] Snapshots sent at 20 Hz; < 50 KB/s per player
- [ ] All voxel changes persisted to PostgreSQL
- [ ] Server restarts cleanly; world state restored

**Deployment:**
- [ ] Dockerfile builds & runs
- [ ] docker-compose.yaml test environment works
- [ ] Prometheus metrics scrape correctly
- [ ] Server health check endpoint responds
- [ ] Logs are readable and useful

---

## Risks & Mitigations

### 1. Voxel Delta Replication Bandwidth

**Risk:** Voxel destruction events accumulate; broadcasting all deltas to 50 players consumes excessive bandwidth.

**Baseline Calculation:**
- Mining throughput: 10 voxels/sec per player (realistic rate)
- 50 players × 10 voxels = 500 voxel deltas/sec server-wide
- Delta message size: 5 voxels per snapshot (10 Hz aggregation)
- Snapshot size per player: 5 × 7 bytes (world_x/y/z + type) = 35 bytes
- 50 players × 20 snapshots/sec × 35 bytes ≈ **35 KB/s** (well within budget)

**Mitigation:**
- Aggregate voxel deltas over 50 ms windows (matches 20 Hz snapshot rate)
- Only send deltas to players within interest radius (not all)
- Use spatial partitioning: sector-level interest (player only needs deltas in sector)
- If bandwidth exceeds budget: lower mining rate or reduce player count

### 2. Voxel Chunk Storage Growth

**Risk:** Over time, voxel_events and voxel_chunks tables grow unbounded.

**Calculation:**
- 4×4 sectors, each 512×512×256 voxels = 16 chunks per axis = 4,096 chunks total
- ~50 MB per sector compressed = 200 MB/sector × 16 sectors = 3.2 GB worst-case
- 10 voxels destroyed/sec × 50 players = 500 deltas/sec → 1.5 GB/month in event log

**Mitigation:**
- Set voxel_events retention: keep 30 days; archive older events
- Periodically consolidate voxel_events into chunk_snapshots (replace event log with final state)
- Implement chunk versioning: old versions deleted after consolidation
- Database size monitoring via Prometheus (alert if > 10 GB)
- Optional: Time-series DB (InfluxDB) for events instead of PostgreSQL

### 3. Server Tick Instability Under Load

**Risk:** With 50 players, tick duration creeps above 16.67 ms budget.

**Hotspots:**
- Entity iteration: O(N) per tick; 5,000 entities per sector × 60 ticks = 300,000 ops
- Physics (AABB overlap checks): O(N²) without spatial partitioning
- Chunk mesh regeneration: ~2 ms per chunk
- Database writes (every 1 sec): blocking query on slow disk

**Mitigation:**
- Spatial partitioning: Divide each sector into 8×8 grid; only test collisions within cell
- Async database writes: Queue deltas, flush to DB in separate thread
- Mesh generation: Move to worker thread; upload async
- Profiling at 50 players: Measure each phase; cap frequency if needed
- Tick timeout: Log warning if tick > 20 ms; skip non-critical work (e.g., defer mesh rebuild by 1 frame)

### 4. Voxel Mesh Rebuild Cost

**Risk:** Greedy mesh generation takes too long when large voxel patches destroyed.

**Example:** Player detonates 1,000-voxel bomb; 8 adjacent chunks marked dirty; 8 mesh rebuilds @ 1 ms each = 8 ms in a single tick.

**Mitigation:**
- Pacing: Rebuild only 2 dirty chunks per tick (batch over frames)
- Look-ahead: If dirty count > threshold, prioritize by distance to camera
- GPU compute: Use compute shader to rebuild meshes (future optimization)
- Fallback: If rebuild backlog > 20 chunks, render simple wireframe LOD until caught up

### 5. Network Packet Loss & Latency

**Risk:** Player input lost; server snapshot dropped → desync between client and server.

**Scenario:** UDP packet loss rate 2% on player's ISP; player sends 60 commands/sec = 1.2 lost/sec; client reconciliation fails.

**Mitigation:**
- Input resend: Client resends command if no snapshot received for 250 ms
- Snapshot redundancy: Send snapshots 20 Hz, but client accepts older snapshots
- Sequence numbers: Track which snap client last ack'd; only send deltas from that point
- Graceful degradation: At 5%+ loss, reduce snapshot frequency to 10 Hz to reduce overhead
- Telemetry: Log per-player packet loss; alert if sustained > 3%

### 6. Player Login Thundering Herd

**Risk:** 50 players all login at 9 AM; database connections spike; server OOM.

**Scenario:** Connection pool max 20; 50 login requests queue up; PostgreSQL handle creation floods RAM.

**Mitigation:**
- Connection pool: Set max_pool_size = 30; queue requests with timeout (5 sec)
- Staged login: Authenticate, load metadata (fast); load full entity list (async)
- Client-side backoff: If login fails, wait random 1–5 seconds before retry
- Monitoring: Alert if connection pool utilization > 80%

### 7. Sector Boundary Transitions

**Risk:** Player moves from Sector (0,0) to (0,1); ships at boundary visible to both; syncing becomes complex.

**Mitigation:**
- Boundary zones: Sector boundaries are 1-chunk thick "overlap" where entities owned by either sector
- Interest radius includes boundary: When loading chunks, also fetch edge chunks from adjacent sectors
- Ownership rule: Entity "belongs" to sector of its center position; messages route accordingly
- Lazy migration: When entity moves to new sector, update owner reference lazily (next tick)

### 8. Cheating / Client Prediction Abuse

**Risk:** Client predicts ship position beyond server's validated range; player exploits desync to attack safely.

**Mitigation:**
- Server validation: Every command checked for distance, trajectory plausibility
- Snapshot reconciliation: Client position may diverge; server snap brings back to truth
- Input validation: Command to move 100 units/sec rejected if max speed is 50 units/sec
- Replay detection: Track last 10 snapshots; if player claims to be far from all, reject
- Logging: Suspicious commands logged for audit

---

## Conclusion

This architecture provides a solid foundation for a voxel-based isometric RTS MMO:

- **Authoritative server** ensures all gameplay is fair and consistent
- **60 Hz tick rate** with client prediction balances responsiveness and network load
- **PostgreSQL persistence** keeps the shared world intact across server restarts
- **Interest management** scales from 50 to hundreds of players
- **Directx 12 rendering** with greedy meshing achieves low-poly voxel aesthetic
- **Containerized deployment** makes shipping and scaling straightforward

**Next Steps:**
1. Validate database schema with sample data (10 sectors, 1,000,000 voxel events)
2. Prototype greedy meshing algorithm; measure mesh generation time
3. Test network codec with packet loss simulation (2–5% drop rate)
4. Build server tick loop; measure tick time at 50 concurrent players
5. Implement first two weeks (foundation + network); get playable build in hands early

---

**Document Version:** 1.0  
**Last Updated:** March 2026  
**Author:** StarStrike Architecture Team  
**Status:** Ready for Implementation
