#pragma once
// coop_protocol.h: Cooperative multiplayer protocol definitions
//
// Protocol version and message types for XRay Coop Mod
//

#include "xrCommon/xr_types.h"

namespace coop
{
// Protocol version - increment when making breaking changes
constexpr u32 COOP_PROTOCOL_VERSION = 1;

// Maximum supported players in co-op session
constexpr u32 COOP_MAX_PLAYERS = 4;

// Network update rates (Hz)
constexpr u32 COOP_PLAYER_UPDATE_RATE = 20;   // Player state updates per second
constexpr u32 COOP_AI_UPDATE_RATE = 10;       // AI NPC updates per second
constexpr u32 COOP_ENTITY_UPDATE_RATE = 5;    // Generic entity updates per second

// Default network port for co-op sessions
constexpr u16 COOP_DEFAULT_PORT = 5445;

// Interpolation settings
constexpr u32 COOP_INTERP_BUFFER_SIZE = 5;    // Number of snapshots to buffer
constexpr u32 COOP_INTERP_DELAY_MS = 100;     // Default interpolation delay

// Network ID ranges
constexpr u32 COOP_NETID_INVALID = 0;
constexpr u32 COOP_NETID_PLAYER_MIN = 1;
constexpr u32 COOP_NETID_PLAYER_MAX = 64;
constexpr u32 COOP_NETID_ENTITY_MIN = 65;

// Co-op message types are defined in xrServerEntities/xrMessages.h
// Starting at M_COOP_BASE (0x1000)

// Join response codes
enum EJoinResponseCode : u8
{
    JOIN_OK = 0,
    JOIN_DENIED_FULL,             // Session is full
    JOIN_DENIED_VERSION,          // Protocol version mismatch
    JOIN_DENIED_LEVEL,            // Level mismatch
    JOIN_DENIED_BANNED,           // Player is banned
    JOIN_DENIED_PASSWORD,         // Wrong password
    JOIN_DENIED_OTHER             // Other reason
};

// Player state flags (bitfield)
enum ECoopPlayerFlags : u16
{
    COOP_PLAYER_FLAG_NONE       = 0,
    COOP_PLAYER_FLAG_HOST       = (1 << 0),   // This player is the host
    COOP_PLAYER_FLAG_READY      = (1 << 1),   // Player ready to play
    COOP_PLAYER_FLAG_ALIVE      = (1 << 2),   // Player is alive
    COOP_PLAYER_FLAG_DOWNED     = (1 << 3),   // Player is downed (needs revive)
    COOP_PLAYER_FLAG_SPECTATING = (1 << 4),   // Player is spectating
};

// Movement state flags (matches engine mcXXX flags)
enum ECoopMoveFlags : u16
{
    COOP_MOVE_FWD       = (1 << 0),
    COOP_MOVE_BACK      = (1 << 1),
    COOP_MOVE_LEFT      = (1 << 2),
    COOP_MOVE_RIGHT     = (1 << 3),
    COOP_MOVE_CROUCH    = (1 << 4),
    COOP_MOVE_SPRINT    = (1 << 5),
    COOP_MOVE_JUMP      = (1 << 6),
    COOP_MOVE_AIM       = (1 << 7),
    COOP_MOVE_FIRE      = (1 << 8),
    COOP_MOVE_FIRE2     = (1 << 9),
    COOP_MOVE_RELOAD    = (1 << 10),
    COOP_MOVE_USE       = (1 << 11),
};

// Snapshot ID type for ordering
using SnapshotID = u32;

// Packed player input structure
#pragma pack(push, 1)
struct CoopPlayerInput
{
    u32 timestamp;              // Client timestamp
    u16 move_flags;             // ECoopMoveFlags
    float yaw;                  // View yaw angle
    float pitch;                // View pitch angle
    u8 active_slot;             // Active weapon slot
    
    CoopPlayerInput()
        : timestamp(0), move_flags(0), yaw(0.0f), pitch(0.0f), active_slot(0) {}
};

struct CoopPlayerSnapshot
{
    SnapshotID snapshot_id;     // Monotonic snapshot ID
    u32 timestamp;              // Server timestamp
    u32 net_id;                 // Player's network ID
    
    // Transform
    float pos_x, pos_y, pos_z;
    float vel_x, vel_y, vel_z;
    float yaw, pitch, roll;
    
    // State
    float health;
    float stamina;
    float radiation;
    
    u16 move_flags;             // Movement state
    u16 player_flags;           // ECoopPlayerFlags
    u8 active_slot;             // Equipped weapon slot
    u16 anim_state;             // Animation state ID
    
    CoopPlayerSnapshot()
        : snapshot_id(0), timestamp(0), net_id(0)
        , pos_x(0), pos_y(0), pos_z(0)
        , vel_x(0), vel_y(0), vel_z(0)
        , yaw(0), pitch(0), roll(0)
        , health(1.0f), stamina(1.0f), radiation(0)
        , move_flags(0), player_flags(0), active_slot(0), anim_state(0) {}
};

struct CoopEntitySnapshot
{
    SnapshotID snapshot_id;
    u32 timestamp;
    u32 net_id;
    u16 entity_type;            // Entity class ID
    
    // Transform
    float pos_x, pos_y, pos_z;
    float yaw, pitch, roll;
    
    // Basic state
    float health;
    u8 state_flags;
    
    CoopEntitySnapshot()
        : snapshot_id(0), timestamp(0), net_id(0), entity_type(0)
        , pos_x(0), pos_y(0), pos_z(0)
        , yaw(0), pitch(0), roll(0)
        , health(1.0f), state_flags(0) {}
};

struct CoopFireEvent
{
    u32 timestamp;
    u32 shooter_net_id;
    u16 weapon_id;              // Weapon section hash
    
    // Fire origin and direction
    float origin_x, origin_y, origin_z;
    float dir_x, dir_y, dir_z;
    
    u8 fire_mode;               // 0=single, 1=auto, 2=burst
    u8 ammo_type;
    
    CoopFireEvent()
        : timestamp(0), shooter_net_id(0), weapon_id(0)
        , origin_x(0), origin_y(0), origin_z(0)
        , dir_x(0), dir_y(0), dir_z(1)
        , fire_mode(0), ammo_type(0) {}
};

struct CoopDamageEvent
{
    u32 timestamp;
    u32 target_net_id;          // Who got hit
    u32 source_net_id;          // Who caused damage (0 = environment)
    
    float damage;               // Damage amount
    float impulse;              // Physics impulse
    
    // Hit location
    float hit_x, hit_y, hit_z;
    float dir_x, dir_y, dir_z;
    
    u16 hit_type;               // Damage type (bullet, explosion, etc.)
    u16 bone_id;                // Hit bone ID
    
    CoopDamageEvent()
        : timestamp(0), target_net_id(0), source_net_id(0)
        , damage(0), impulse(0)
        , hit_x(0), hit_y(0), hit_z(0)
        , dir_x(0), dir_y(0), dir_z(1)
        , hit_type(0), bone_id(0) {}
};

struct CoopPingMarker
{
    u32 timestamp;
    u32 placer_net_id;          // Who placed the marker
    
    float pos_x, pos_y, pos_z;
    u8 marker_type;             // 0=generic, 1=enemy, 2=loot, 3=danger
    
    CoopPingMarker()
        : timestamp(0), placer_net_id(0)
        , pos_x(0), pos_y(0), pos_z(0)
        , marker_type(0) {}
};

struct CoopChatMessage
{
    u32 timestamp;
    u32 sender_net_id;
    u8 message_length;
    // Followed by message_length bytes of UTF-8 text
    
    CoopChatMessage()
        : timestamp(0), sender_net_id(0), message_length(0) {}
};

struct CoopSessionInfo
{
    u32 protocol_version;
    u32 level_crc;              // Level checksum for validation
    u8 max_players;
    u8 current_players;
    u8 password_required;
    char level_name[64];
    char host_name[32];
    
    CoopSessionInfo()
        : protocol_version(COOP_PROTOCOL_VERSION)
        , level_crc(0)
        , max_players(COOP_MAX_PLAYERS)
        , current_players(1)
        , password_required(0)
    {
        level_name[0] = '\0';
        host_name[0] = '\0';
    }
};

#pragma pack(pop)

// Helper to check if a network ID is for a player
inline bool IsPlayerNetID(u32 net_id)
{
    return net_id >= COOP_NETID_PLAYER_MIN && net_id <= COOP_NETID_PLAYER_MAX;
}

// Helper to check if a network ID is for an entity
inline bool IsEntityNetID(u32 net_id)
{
    return net_id >= COOP_NETID_ENTITY_MIN;
}

} // namespace coop
