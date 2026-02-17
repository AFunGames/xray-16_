# Cooperative Multiplayer Mod Architecture

## Overview

This document describes the architecture of the cooperative multiplayer mod for XRay Engine 1.6 (S.T.A.L.K.E.R.). The mod enables 2-4 players to explore the Zone together in a host-authoritative session.

## Design Principles

1. **Host-Authoritative**: The host (listen server) owns the game simulation
2. **Minimal Intrusion**: Changes are localized and reversible  
3. **Single-Player Compatible**: Co-op is enabled via config flag; SP builds unaffected
4. **Determinism Where Possible**: Server validates all actions

## Module Structure

```
src/xrGame/xr_coop/
├── coop_protocol.h         # Protocol version & message definitions
├── coop_net_entity.h       # NetEntity interface for replicable objects
├── coop_net_entity.cpp     
├── coop_session_manager.h  # Session lifecycle (host/join/disconnect)
├── coop_session_manager.cpp
├── coop_player_state.h     # Player state for replication
├── coop_player_state.cpp
├── coop_interpolator.h     # Client-side interpolation
├── coop_interpolator.cpp
├── coop_damage_handler.h   # Server-side hit validation
├── coop_damage_handler.cpp
├── coop_ai_sync.h          # AI entity sync (server-owned)
├── coop_ai_sync.cpp
├── coop_debug_overlay.h    # Network stats overlay
├── coop_debug_overlay.cpp
├── game_sv_coop.h          # Server-side coop game state
├── game_sv_coop.cpp
├── game_cl_coop.h          # Client-side coop game state
├── game_cl_coop.cpp
└── CMakeLists.txt
```

## Key Classes

### Protocol & Messages (`coop_protocol.h`)

- Protocol version constant for compatibility checking
- New message types:
  - `M_COOP_PLAYER_INPUT` - Client input RPC
  - `M_COOP_PLAYER_STATE` - Player state snapshot
  - `M_COOP_ENTITY_STATE` - Entity state snapshot
  - `M_COOP_DAMAGE_EVENT` - Hit/damage events
  - `M_COOP_AI_STATE` - AI NPC state
  - `M_COOP_PING_MARKER` - Player ping/marker
  - `M_COOP_CHAT` - Chat messages

### NetEntity Interface (`coop_net_entity.h`)

```cpp
class ICoopNetEntity {
public:
    virtual u32  GetNetID() const = 0;
    virtual void SetNetID(u32 id) = 0;
    virtual void WriteSnapshot(NET_Packet& P) = 0;
    virtual void ReadSnapshot(NET_Packet& P) = 0;
    virtual bool IsDirty() const = 0;
    virtual void ClearDirty() = 0;
    virtual float GetReplicationPriority(const Fvector& viewerPos) const = 0;
};
```

### Session Manager (`coop_session_manager.h`)

Responsibilities:
- Host session creation with level and options
- Client connection/disconnection handling
- Player spawn/despawn coordination
- Level transition management

### Player State (`coop_player_state.h`)

Replicated state:
- Position, rotation, velocity
- Animation state ID
- Health, stamina, radiation
- Active weapon ID, ammo count
- Movement state flags (crouch, sprint, etc.)

### Interpolator (`coop_interpolator.h`)

Client-side smoothing:
- Buffered state history (3-5 snapshots)
- Linear interpolation between snapshots
- Extrapolation for high-latency situations
- Configurable interpolation delay

### Damage Handler (`coop_damage_handler.h`)

Server-side validation:
- Client sends fire event + direction
- Server performs raycast validation
- Server applies damage and broadcasts result
- Basic anti-cheat (distance, rate limiting)

### AI Sync (`coop_ai_sync.h`)

Server-owned AI:
- Server simulates all AI entities
- Clients receive position/animation updates
- Client AI is "visual only" (no simulation)
- Prioritized by distance to nearest player

### Game States

#### `game_sv_coop` (Server)
- Extends `game_sv_Single` to leverage A-life system
- Manages multiple players
- Owns world simulation
- Creates and broadcasts snapshots

#### `game_cl_coop` (Client)
- Extends `game_cl_Single` for SP-like experience
- Receives and applies snapshots
- Handles local input prediction (optional)
- Renders remote players

## Data Flow

### Client → Server (Input)
```
1. Client captures input (WASD, mouse, fire)
2. Package into M_COOP_PLAYER_INPUT
3. Send to server (unreliable, frequent)
4. Server applies to authoritative player state
```

### Server → Client (State)
```
1. Server ticks world simulation (AI, physics, etc.)
2. Collects dirty entities
3. Prioritizes by distance/relevance
4. Packages into M_COOP_ENTITY_STATE / M_COOP_PLAYER_STATE
5. Broadcasts to all clients
6. Clients apply snapshots + interpolate
```

### Damage Flow
```
1. Client fires weapon → M_COOP_DAMAGE_EVENT (fire direction)
2. Server validates (raycast, ammo check)
3. Server applies damage to target
4. Server broadcasts damage result
5. All clients play hit effects
```

## Replication Model

### Network ID Assignment
- Server assigns stable 32-bit IDs
- ID 0 reserved for invalid
- Players get IDs in range 1-64
- Entities get IDs starting at 65

### Update Rates
- Player state: 20 Hz (critical)
- AI NPCs: 10 Hz (important)
- Props/items: 5 Hz (low priority)
- Static entities: On change only

### Reliability Rules
| Message Type | Reliability | Notes |
|--------------|-------------|-------|
| Player input | Unreliable | High frequency, loss OK |
| Player state | Unreliable | Superseded by newer |
| Damage events | Reliable | Must be delivered |
| Spawn/despawn | Reliable | Critical |
| Chat | Reliable | Must be delivered |

## Configuration

### Console Variables
```
coop_enabled 1              # Enable co-op mode
coop_max_players 4          # Maximum players (2-4)
coop_host_port 5445         # Host listen port
coop_interp_delay 100       # Interpolation delay (ms)
coop_debug_overlay 0        # Show debug overlay
```

### Command Line
```
-coop                       # Enable co-op mode
-coop_host                  # Start as host
-coop_connect <ip:port>     # Connect to host
```

## File Changes Summary

### Modified Files
- `src/xrServerEntities/xrMessages.h` - Add new message types
- `src/xrServerEntities/game_base_space.h` - Add game ID for coop
- `src/xrGame/game_type.h` - Register coop game type
- `src/xrGame/Level.h/cpp` - Hook coop initialization
- `src/xrGame/Actor_Network.cpp` - Coop-specific export/import
- `src/xrGame/console_commands.cpp` - Add coop commands
- `src/xrGame/CMakeLists.txt` - Include xr_coop module

### New Files
- `src/xrGame/xr_coop/*` - All coop module files

## Test Plan

### Basic Connection Test
1. Start game with `-coop -coop_host`
2. Load escape level
3. Second instance: `-coop -coop_connect localhost:5445`
4. Verify both players appear in level

### Movement Sync Test
1. Connect two players
2. Move player 1 around
3. Verify player 2 sees smooth movement
4. Test crouch, sprint, jump states

### Combat Test
1. Spawn test NPC (boar)
2. Player 1 shoots NPC
3. Verify damage applied
4. Verify player 2 sees hit effects

## Known Limitations (MVP)

1. No full quest/story sync
2. No physics sync for all objects
3. Maximum 4 players
4. No save/load in co-op
5. No anti-cheat
6. AI may occasionally desync
7. Some scripted sequences may break
