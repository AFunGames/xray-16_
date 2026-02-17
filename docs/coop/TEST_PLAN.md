# Cooperative Multiplayer Test Plan

## Overview

This document provides step-by-step test procedures for validating the cooperative multiplayer mod functionality.

## Prerequisites

1. Build the engine with co-op support enabled
2. Have access to game data files (levels, models, etc.)
3. Two instances of the game (can be on same machine for local testing)
4. Network connectivity between test machines (if testing remotely)

## Build Instructions

### Windows (Visual Studio 2019/2022)

```batch
# Clone repository
git clone https://github.com/OpenXRay/xray-16.git
cd xray-16

# Generate Visual Studio solution
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release

# Or open build/OpenXRay.sln in Visual Studio
```

### Linux (GCC/Clang)

```bash
# Install dependencies
sudo apt-get install cmake gcc g++ libsdl2-dev libopenal-dev

# Generate makefiles
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)
```

## Test Scenarios

### Test 1: Host Session Creation

**Objective**: Verify host can start a co-op session

**Steps**:
1. Launch the game with co-op enabled:
   ```
   xray.exe -coop -coop_host -start coop/escape
   ```
2. Open console (~) and verify:
   - `coop_enabled` shows `1`
   - No error messages related to co-op
3. Load a level (e.g., Escape)
4. Verify actor spawns correctly
5. Check debug overlay (`coop_debug_toggle`):
   - Session state should show "HOSTING"
   - Players: 1/4

**Expected Result**: Host session starts successfully, single player in session

---

### Test 2: Client Connection

**Objective**: Verify client can connect to host

**Prerequisites**: Host session running (Test 1)

**Steps**:
1. On second machine/instance, launch:
   ```
   xray.exe -coop -coop_connect localhost:5445
   ```
2. Wait for connection and level loading
3. Verify client loads same level as host
4. Check debug overlay on both:
   - Both show "ACTIVE" state
   - Players: 2/4 on both

**Expected Result**: Client connects and joins session

---

### Test 3: Player Movement Synchronization

**Objective**: Verify player movement is synchronized between clients

**Prerequisites**: Two players connected (Test 2)

**Steps**:
1. On Host: Move the player forward (W key)
2. On Client: Observe host's player proxy
3. Verify:
   - Remote player moves smoothly
   - No teleporting or jitter
   - Movement direction matches input
4. Repeat with:
   - Backward movement (S)
   - Strafing (A/D)
   - Crouching (C)
   - Sprinting (Shift+W)
   - Jumping (Space)
5. On Client: Move around
6. On Host: Verify client's movement is visible

**Expected Result**: Movement is synchronized with smooth interpolation

---

### Test 4: Player Rotation Synchronization

**Objective**: Verify player view angles are synchronized

**Steps**:
1. On Host: Look around (mouse movement)
2. On Client: Observe host player proxy orientation
3. Verify:
   - Player proxy faces correct direction
   - Rotation is smooth, not jerky
4. Test edge cases:
   - Spinning quickly
   - Looking straight up/down

**Expected Result**: Rotation is synchronized smoothly

---

### Test 5: Weapon Fire Synchronization

**Objective**: Verify shooting is visible on all clients

**Steps**:
1. Equip a weapon on Host
2. Fire the weapon
3. On Client, verify:
   - Gunshot sound is heard
   - Muzzle flash is visible
   - Bullet tracer (if applicable) is visible
4. Repeat with Client firing, Host observing

**Expected Result**: Fire events are synchronized

---

### Test 6: Damage Synchronization

**Objective**: Verify damage is properly replicated

**Prerequisites**: Test NPC spawned (boar or bandit)

**Steps**:
1. On Host: Spawn test NPC
   ```
   console: spawn boar
   ```
2. On Host: Shoot the NPC
3. Verify:
   - NPC takes damage (health decreases)
   - Hit reaction animation plays
   - Both players see the same result
4. On Client: Shoot the NPC
5. Verify:
   - Server validates hit
   - Damage applied
   - NPC dies when health reaches 0

**Expected Result**: Damage is server-validated and synchronized

---

### Test 7: AI Synchronization

**Objective**: Verify AI entities are synchronized

**Steps**:
1. Find or spawn an AI NPC
2. Observe NPC behavior on both clients:
   - Position matches
   - Animation matches
   - Movement is smooth
3. Aggro the NPC (shoot near it)
4. Verify combat state syncs:
   - Both players see NPC in combat
   - NPC targets are consistent

**Expected Result**: AI state is synchronized from server

---

### Test 8: Chat System

**Objective**: Verify chat messages are delivered

**Steps**:
1. On Host: Open chat (T key or console)
2. Type a message and send
3. On Client: Verify message received
4. On Client: Send a reply
5. On Host: Verify reply received

**Expected Result**: Chat messages delivered to all players

---

### Test 9: Ping Marker System

**Objective**: Verify ping markers are visible to all players

**Steps**:
1. On Host: Use ping key (Middle mouse or custom bind)
2. Aim at a location and ping
3. On Client: Verify:
   - Ping marker visible at correct world position
   - Marker shows who placed it
4. Repeat with Client placing ping

**Expected Result**: Ping markers visible to all players

---

### Test 10: Player Disconnect/Reconnect

**Objective**: Verify graceful disconnect handling

**Steps**:
1. With two players connected
2. On Client: Disconnect (Alt+F4 or menu)
3. On Host: Verify:
   - Player removed from player list
   - No errors or crashes
   - Game continues normally
4. On Client: Reconnect
5. Verify client can rejoin session

**Expected Result**: Disconnect handled gracefully

---

### Test 11: Debug Overlay

**Objective**: Verify debug overlay displays correctly

**Steps**:
1. Enable debug overlay: `coop_debug_toggle`
2. Verify displayed information:
   - Session state
   - Player count
   - RTT (round-trip time)
   - Packets/second
   - Entity count
   - Snapshot IDs
3. Generate network traffic
4. Verify counters update
5. Intentionally cause lag (if possible)
6. Verify RTT color changes (green→yellow→red)

**Expected Result**: Debug overlay shows accurate statistics

---

### Test 12: Network Stress Test

**Objective**: Verify system handles high activity

**Steps**:
1. Connect maximum players (4)
2. All players move simultaneously
3. All players fire weapons
4. Spawn multiple AI NPCs
5. Monitor:
   - Frame rate
   - Network bandwidth (debug overlay)
   - Entity count
6. Verify no crashes or major desync

**Expected Result**: System remains stable under load

---

## Known Limitations

1. **Quest/Story Sync**: Scripted story sequences are not synchronized. Only one player should interact with quest triggers.

2. **Physics Objects**: Not all physics props are synchronized. Important items are synced; decorative objects may differ between clients.

3. **Save/Load**: Game saves are not supported in co-op mode. Session state is memory-only.

4. **Maximum Players**: Hard limit of 4 players for MVP.

5. **AI Pathfinding**: AI may occasionally path differently on client due to interpolation timing.

6. **Level Transitions**: Changing levels requires all players to be in sync. Stragglers may be teleported.

7. **Inventory**: Only equipped weapon is synchronized. Full inventory sync is not implemented.

8. **Anti-Cheat**: No anti-cheat protection. Trust all connected clients.

9. **Voice Chat**: Not implemented. Use external voice chat.

10. **Matchmaking**: No lobby/matchmaking system. Direct IP connection only.

## Troubleshooting

### Connection Fails
- Verify firewall allows port 5445 (UDP)
- Check host IP address is correct
- Verify both using same game version

### Desync Warnings
- Check network stability
- Reduce player count
- Verify server has adequate CPU

### Performance Issues
- Reduce update rates in config
- Limit entity count
- Use lower detail settings

## Console Commands Reference

| Command | Description |
|---------|-------------|
| `coop_enabled [0/1]` | Enable/disable co-op mode |
| `coop_debug_toggle` | Toggle debug overlay |
| `coop_host <level>` | Start hosting session |
| `coop_connect <ip:port>` | Connect to host |
| `coop_disconnect` | Leave current session |
| `coop_players` | List connected players |
| `coop_kick <player>` | Kick player (host only) |
| `coop_chat <message>` | Send chat message |
