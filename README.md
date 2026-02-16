# StarStrike.RTS

A client/server space-RTS MMO prototype built on the Neuron engine framework
with DirectX 12 rendering, UDP networking, and a continuous open-world
architecture.

## Prerequisites

- **Windows 11** (or Windows 10 21H2+)
- **Visual Studio 2026** with:
  - Desktop development with C++
  - Windows SDK 10.0.26100 or later
  - C++/WinRT NuGet package (restored automatically)
- NuGet packages restore on build (packages listed in `packages.config`)

## Build

1. Open `StarStrike.slnx` in Visual Studio 2026.
2. Select **Debug | x64** configuration.
3. **Build → Rebuild Solution** (or `Ctrl+Shift+B`).

All six projects build:

| Project | Output | Type |
|---------|--------|------|
| NeuronCore | `NeuronCore.lib` | Static library |
| GameLogic | `GameLogic.lib` | Static library |
| NeuronClient | `NeuronClient.lib` | Static library |
| NeuronServer | `NeuronServer.lib` | Static library |
| Server | `Server.exe` | Console application |
| StarStrike | `StarStrike.exe` | Win32 application |

### Command-line build

```
msbuild StarStrike.slnx /t:Rebuild /p:Configuration=Debug /p:Platform=x64
```

## Run

### 1. Start the server

```
cd x64\Debug
Server.exe
```

The server listens on UDP port **27015** by default.
Pass a custom port as the first argument: `Server.exe 27016`.

### 2. Start clients

Launch `StarStrike.exe` (from the `x64\Debug\StarStrike\` folder or via
Visual Studio F5). It automatically connects to `127.0.0.1:27015`.

Start a second instance to see two ships in the same world.

### 3. Play

- **Right-click** — issue a MoveTo command at the cursor position.
- **S key** — stop the ship.
- The camera follows the local player's ship.

## Architecture

See [Architecture.md](Architecture.md) for the full design document covering:

- Open world model
- Type-driven object system
- Networking protocol & snapshot replication
- Client prediction & reconciliation
- Rendering pipeline
- Scaling strategy

## Project Layout

```
NeuronCore/          Foundation: math, debug, file system, WorldTypes, NetProtocol
GameLogic/           World simulation: WorldObject, World, ShipDefs
NeuronServer/        Server networking: ServerNet, GameServer
NeuronClient/        DX12 engine + ClientNet
Server/              Headless server entry point
StarStrike/          Game client: GameApp, ClientWorld, WorldRenderer
  Shaders/           HLSL source (SM 6.x) — reference copies
  CompiledShaders/   Pre-compiled shader bytecode headers
  Assets/            Fonts, logos, textures
```

## Docker (Windows Server Core)

Build and run the server in a container:

```
docker build -t starstrike-server -f Dockerfile .
docker run -p 27015:27015/udp starstrike-server
```

Requires Windows containers mode in Docker Desktop.

## Non-Goals (MVP)

- No persistence / save system
- No matchmaking or lobby
- No sound / music
- No advanced UI / HUD
- No combat (projectile type is a placeholder)
