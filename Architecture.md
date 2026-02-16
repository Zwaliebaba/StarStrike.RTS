# StarStrike.RTS — Architecture

## Overview

StarStrike is a client/server space-RTS MMO built on the Neuron engine framework.
The architecture is designed for a **single continuous open world** with a
**generic, type-driven entity system** that scales to many object types and
thousands of instances.

---

## Project Structure

```
StarStrike.RTS/
├── NeuronCore/        Static lib — shared types, math, serialization, protocol
├── GameLogic/         Static lib — authoritative world simulation (no networking)
├── NeuronClient/      Static lib — DX12 graphics engine + client networking
├── NeuronServer/      Static lib — server networking + game server loop
├── Server/            Console app — headless server executable
├── StarStrike/        Win32 app  — DirectX 12 game client
└── StarStrike.slnx    Solution file
```

### Dependency Graph

```
NeuronCore  ←── GameLogic
    ↑              ↑
NeuronClient  NeuronServer
    ↑              ↑
StarStrike      Server
```

---

## Open World Model

### Unified Coordinate System
All objects exist in a single continuous 3D world defined by a configurable
square boundary (default 2000×2000 units on the XZ plane, Y=0 for the ground
plane). Positions are `XMFLOAT3`.

### No Zoning (MVP)
The MVP uses a single `World` instance on the server. The architecture supports
future partitioning:

- The `World` class can be instanced per zone/shard.
- `ObjectId` is globally unique (monotonic `uint32_t`).
- Snapshot packets include all objects; a future spatial-interest system would
  filter by proximity.

### World Bounds
Objects are clamped to the world boundary each tick. The boundary size is set
at `World::Startup(float worldSize)`.

---

## Object System

### Core Types (`NeuronCore/WorldTypes.h`)

| Type | Description |
|------|-------------|
| `ObjectId` | `uint32_t`, 0 = invalid |
| `WorldObjectType` | Enum: Ship, Asteroid, Crate, JumpGate, Projectile, Station, Turret |
| `ShipClass` | Enum: Asteria, Aurora, Avalanche |
| `ObjectFlags` | Bitmask: Active, Moving |
| `ObjectState` | POD struct: id, type, subclass, flags, position, velocity, yaw, hitpoints |

### WorldObject (`GameLogic/WorldObject.h`)

Each world object wraps an `ObjectState` plus simulation data (target position,
owner client). The `Update(deltaT)` method dispatches by `ObjectType`:

- **Ship** — Accelerates toward target, decelerates to stop, respects per-class
  speed/acceleration from `ShipDefs.h`.
- **All others** — Static (no-op update in MVP).

### Ship Definitions (`GameLogic/ShipDefs.h`)

Static data per `ShipClass`:

| Class | Speed | Accel | Turn | HP | Radius |
|-------|-------|-------|------|----|--------|
| Asteria | 50 | 20 | 2.0 | 100 | 5 |
| Aurora | 70 | 30 | 3.0 | 80 | 4 |
| Avalanche | 40 | 15 | 1.5 | 150 | 7 |

### Scaling Strategy

Adding a new object type requires:

1. Add enum value to `WorldObjectType`.
2. Add a `case` in `WorldObject::Update()` (or a function-table entry).
3. Add mesh data in `WorldRenderer::Startup()`.
4. Optionally add a definition table (like `ShipDefs.h`).

No core systems need rewriting. The world iterates
`std::unordered_map<ObjectId, WorldObject>` generically.

---

## Networking & MMO Model

### Transport
Raw WinSock UDP. Non-blocking sockets on both client and server.

### Protocol (`NeuronCore/NetProtocol.h`)

All packets start with `PacketHeader`:
```
uint32_t protocolId   — magic "STR1" (0x53545231)
uint8_t  type         — PacketType enum
uint16_t sequence     — sender sequence number
uint16_t ack          — last received remote sequence
uint32_t ackBits      — bitmask of 32 prior acks
```

| Packet | Direction | Purpose |
|--------|-----------|---------|
| Connect | C→S | Request connection |
| ConnectAck | S→C | Assign clientId + playerObjectId |
| Disconnect | C→S | Clean disconnect |
| ClientInput | C→S | MoveTo / Stop commands |
| WorldSnapshot | S→C | Full world state at 10 Hz |
| Heartbeat | C→S | Keepalive |

### Snapshot Replication
- Server sends `WorldSnapshotPacket` at 10 Hz to all clients.
- Each snapshot contains **mixed object types** (ships, asteroids, etc.).
- Per-object: `objectId`, `objectType`, `subclass`, position, velocity, yaw, hp.
- Max 32 objects per packet (fits UDP MTU ~1472 bytes).

### Reliability
The `PacketHeader` includes `ack` + `ackBits` fields for lightweight
reliability. In MVP, snapshots are unreliable (loss-tolerant by design);
`Connect` packets retry on a 1-second timer.

---

## Client Prediction & Reconciliation

### Owned Objects (Ships)
1. Client sends `ClientInput` (MoveTo) to server.
2. Client immediately predicts movement locally.
3. Server applies input authoritatively and broadcasts snapshot.
4. Client receives snapshot with `lastProcessedInput` sequence.
5. Client discards acknowledged inputs and snaps to server state.
6. Unacknowledged inputs are re-applied (target is preserved).

### Remote Objects
- Interpolated between the two most recent snapshot states.
- Interpolation duration = snapshot interval (100 ms).

### Object-Type Awareness
- Only `Ship` objects are predicted.
- Other types are passive (interpolation only).

---

## Rendering

### Pipeline
- DirectX 12 with Shader Model 6.x (DXC-compiled HLSL).
- Root signature: 16 root constants (WorldViewProj matrix).
- PSO: `BasicVS` + `BasicPS` with `VertexPositionColor` input layout.
- Triangle-list primitive topology.

### 80s RTS Aesthetic
- Flat-colored procedural geometry (no textures, no PBR).
- Ships are arrow/wedge shapes; asteroids are octahedrons.
- Strong silhouettes with per-class color coding.
- Fixed isometric camera following the local player.

### Type-Driven Mesh Lookup
`WorldRenderer` stores meshes in a map keyed by `(ObjectType << 8 | subclass)`.
Missing meshes fall back to a default cube. Adding a new visual requires only
adding a `Create*Mesh()` call in `Startup()`.

---

## Server Architecture

### `GameServer` (NeuronServer)
- Owns a `World` instance.
- Fixed-step simulation at 20 Hz.
- Snapshot broadcast at 10 Hz.
- Spawns a ship per connecting client (alternating `ShipClass`).
- Processes `ClientInput` and delegates to `World`.

### Headless Console App
`Server.exe` is a pure console application with no graphics dependencies.
Deployable in Windows Server Core containers.

---

## Future Extension Points

| Feature | Where to Add |
|---------|-------------|
| New object type | `WorldObjectType` enum + `WorldObject::Update` case + mesh |
| Combat / projectiles | `WorldObject::UpdateProjectile()` + collision in `World::Update()` |
| Spatial interest | Filter snapshot objects by client position in `GameServer` |
| Zoning / sharding | Multiple `World` instances + zone-transfer protocol |
| Persistence | Serialize `World` state to disk / database |
| Authentication | Extend `Connect` / `ConnectAck` packets |
