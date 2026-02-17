// coop_player_state.cpp: Implementation of player state for co-op replication
//

#include "StdAfx.h"
#include "coop_player_state.h"
#include "Actor.h"
#include "Inventory.h"
#include "ActorCondition.h"
#include "CharacterPhysicsSupport.h"

namespace coop
{

//-----------------------------------------------------------------------------
// CoopPlayerState
//-----------------------------------------------------------------------------

CoopPlayerState::CoopPlayerState()
    : m_actor(nullptr)
    , m_interp_buffer_head(0)
    , m_interp_buffer_count(0)
    , m_interp_factor(0.0f)
    , m_last_update_time(0)
    , m_last_snapshot_id(0)
{
    // CoopPlayerSnapshot and CoopPlayerInput have default constructors
    // that initialize all members properly - no need for ZeroMemory
    for (u32 i = 0; i < INTERP_BUFFER_SIZE; ++i)
    {
        m_interp_buffer[i] = CoopPlayerSnapshot();
    }
}

CoopPlayerState::~CoopPlayerState()
{
    m_actor = nullptr;
}

void CoopPlayerState::BindToActor(CActor* actor)
{
    m_actor = actor;
    if (actor)
    {
        MarkCoopDirty();
    }
}

u16 CoopPlayerState::GetCoopEntityType() const
{
    // Return a unique type ID for players
    return 0x0001;  // Player type
}

Fvector CoopPlayerState::GetCoopPosition() const
{
    if (m_actor)
    {
        return m_actor->Position();
    }
    return Fvector().set(m_snapshot.pos_x, m_snapshot.pos_y, m_snapshot.pos_z);
}

void CoopPlayerState::WriteCoopSnapshot(NET_Packet& P) const
{
    // Write snapshot data to packet
    P.w_u32(m_snapshot.snapshot_id);
    P.w_u32(m_snapshot.timestamp);
    P.w_u32(m_snapshot.net_id);
    
    // Position
    P.w_float(m_snapshot.pos_x);
    P.w_float(m_snapshot.pos_y);
    P.w_float(m_snapshot.pos_z);
    
    // Velocity
    P.w_float(m_snapshot.vel_x);
    P.w_float(m_snapshot.vel_y);
    P.w_float(m_snapshot.vel_z);
    
    // Rotation
    P.w_float(m_snapshot.yaw);
    P.w_float(m_snapshot.pitch);
    P.w_float(m_snapshot.roll);
    
    // State
    P.w_float(m_snapshot.health);
    P.w_float(m_snapshot.stamina);
    P.w_float(m_snapshot.radiation);
    
    // Flags
    P.w_u16(m_snapshot.move_flags);
    P.w_u16(m_snapshot.player_flags);
    P.w_u8(m_snapshot.active_slot);
    P.w_u16(m_snapshot.anim_state);
}

void CoopPlayerState::ReadCoopSnapshot(NET_Packet& P)
{
    CoopPlayerSnapshot new_snapshot;
    
    new_snapshot.snapshot_id = P.r_u32();
    new_snapshot.timestamp = P.r_u32();
    new_snapshot.net_id = P.r_u32();
    
    // Position
    new_snapshot.pos_x = P.r_float();
    new_snapshot.pos_y = P.r_float();
    new_snapshot.pos_z = P.r_float();
    
    // Velocity
    new_snapshot.vel_x = P.r_float();
    new_snapshot.vel_y = P.r_float();
    new_snapshot.vel_z = P.r_float();
    
    // Rotation
    new_snapshot.yaw = P.r_float();
    new_snapshot.pitch = P.r_float();
    new_snapshot.roll = P.r_float();
    
    // State
    new_snapshot.health = P.r_float();
    new_snapshot.stamina = P.r_float();
    new_snapshot.radiation = P.r_float();
    
    // Flags
    new_snapshot.move_flags = P.r_u16();
    new_snapshot.player_flags = P.r_u16();
    new_snapshot.active_slot = P.r_u8();
    new_snapshot.anim_state = P.r_u16();
    
    // Add to interpolation buffer if this is a remote player
    if (!IsCoopLocallyOwned())
    {
        AddSnapshotToBuffer(new_snapshot);
    }
    
    m_snapshot = new_snapshot;
    m_last_update_time = Device.dwTimeGlobal;
    m_last_snapshot_id = new_snapshot.snapshot_id;
}

void CoopPlayerState::UpdateFromActor()
{
    if (!m_actor)
        return;
    
    m_snapshot.snapshot_id++;
    m_snapshot.timestamp = Device.dwTimeGlobal;
    m_snapshot.net_id = GetCoopNetID();
    
    // Position
    const Fvector& pos = m_actor->Position();
    m_snapshot.pos_x = pos.x;
    m_snapshot.pos_y = pos.y;
    m_snapshot.pos_z = pos.z;
    
    // Velocity
    const Fvector& vel = m_actor->character_physics_support()->movement()->GetVelocity();
    m_snapshot.vel_x = vel.x;
    m_snapshot.vel_y = vel.y;
    m_snapshot.vel_z = vel.z;
    
    // Rotation
    m_snapshot.yaw = m_actor->r_model_yaw;
    m_snapshot.pitch = m_actor->r_torso.pitch;
    m_snapshot.roll = m_actor->r_torso.roll;
    
    // State from actor condition
    CActorCondition& cond = m_actor->conditions();
    m_snapshot.health = cond.GetHealth();
    m_snapshot.stamina = cond.GetPower();
    m_snapshot.radiation = cond.GetRadiation();
    
    // Flags
    m_snapshot.move_flags = static_cast<u16>(m_actor->MovingState());
    m_snapshot.player_flags = COOP_PLAYER_FLAG_ALIVE;
    
    // Active weapon slot
    m_snapshot.active_slot = static_cast<u8>(m_actor->inventory().GetActiveSlot());
    
    // Animation state - would need to get from animation manager
    m_snapshot.anim_state = 0;
    
    m_last_update_time = Device.dwTimeGlobal;
    MarkCoopDirty();
}

void CoopPlayerState::ApplyToActor()
{
    if (!m_actor || IsCoopLocallyOwned())
        return;
    
    // Apply interpolated position
    Fvector pos;
    pos.set(m_snapshot.pos_x, m_snapshot.pos_y, m_snapshot.pos_z);
    
    // Don't teleport if too far - this would cause issues
    float dist = m_actor->Position().distance_to(pos);
    if (dist < 50.0f)  // Reasonable teleport threshold
    {
        m_actor->Position() = pos;
    }
    
    // Apply rotation
    m_actor->r_model_yaw = m_snapshot.yaw;
    m_actor->r_torso.yaw = m_snapshot.yaw;
    m_actor->r_torso.pitch = m_snapshot.pitch;
    m_actor->r_torso.roll = m_snapshot.roll;
    
    // Apply health (for visual state)
    // Note: Server-authoritative, so we don't actually change condition
}

void CoopPlayerState::SetInput(const CoopPlayerInput& input)
{
    m_input = input;
}

void CoopPlayerState::AddSnapshotToBuffer(const CoopPlayerSnapshot& snapshot)
{
    // Add snapshot to circular buffer
    m_interp_buffer[m_interp_buffer_head] = snapshot;
    m_interp_buffer_head = (m_interp_buffer_head + 1) % INTERP_BUFFER_SIZE;
    
    if (m_interp_buffer_count < INTERP_BUFFER_SIZE)
    {
        m_interp_buffer_count++;
    }
}

void CoopPlayerState::InterpolateState(u32 render_time)
{
    if (m_interp_buffer_count < 2)
    {
        // Not enough snapshots to interpolate
        return;
    }
    
    // Calculate interpolation delay target time
    u32 interp_delay = COOP_INTERP_DELAY_MS;
    u32 target_time = render_time - interp_delay;
    
    // Find two snapshots to interpolate between
    int from_idx = -1;
    int to_idx = -1;
    
    // Search through buffer
    for (u32 i = 0; i < m_interp_buffer_count; i++)
    {
        u32 idx = (m_interp_buffer_head - 1 - i + INTERP_BUFFER_SIZE) % INTERP_BUFFER_SIZE;
        
        if (m_interp_buffer[idx].timestamp <= target_time)
        {
            from_idx = idx;
            
            // Find the next snapshot after this
            if (i > 0)
            {
                to_idx = (idx + 1) % INTERP_BUFFER_SIZE;
            }
            break;
        }
    }
    
    if (from_idx < 0 || to_idx < 0)
    {
        // Can't find suitable snapshots - use latest
        m_snapshot = m_interp_buffer[(m_interp_buffer_head - 1 + INTERP_BUFFER_SIZE) % INTERP_BUFFER_SIZE];
        return;
    }
    
    // Calculate interpolation factor
    const CoopPlayerSnapshot& from = m_interp_buffer[from_idx];
    const CoopPlayerSnapshot& to = m_interp_buffer[to_idx];
    
    float time_diff = static_cast<float>(to.timestamp - from.timestamp);
    if (time_diff <= 0.0f)
    {
        m_snapshot = to;
        return;
    }
    
    float t = static_cast<float>(target_time - from.timestamp) / time_diff;
    t = clamp(t, 0.0f, 1.0f);
    
    // Interpolate position
    m_snapshot.pos_x = from.pos_x + (to.pos_x - from.pos_x) * t;
    m_snapshot.pos_y = from.pos_y + (to.pos_y - from.pos_y) * t;
    m_snapshot.pos_z = from.pos_z + (to.pos_z - from.pos_z) * t;
    
    // Interpolate velocity
    m_snapshot.vel_x = from.vel_x + (to.vel_x - from.vel_x) * t;
    m_snapshot.vel_y = from.vel_y + (to.vel_y - from.vel_y) * t;
    m_snapshot.vel_z = from.vel_z + (to.vel_z - from.vel_z) * t;
    
    // Interpolate rotation (simple lerp, should use slerp for better results)
    m_snapshot.yaw = from.yaw + (to.yaw - from.yaw) * t;
    m_snapshot.pitch = from.pitch + (to.pitch - from.pitch) * t;
    m_snapshot.roll = from.roll + (to.roll - from.roll) * t;
    
    // Use latest values for discrete state
    m_snapshot.health = to.health;
    m_snapshot.stamina = to.stamina;
    m_snapshot.radiation = to.radiation;
    m_snapshot.move_flags = to.move_flags;
    m_snapshot.player_flags = to.player_flags;
    m_snapshot.active_slot = to.active_slot;
    m_snapshot.anim_state = to.anim_state;
    
    m_snapshot.snapshot_id = to.snapshot_id;
    m_snapshot.timestamp = target_time;
    m_snapshot.net_id = to.net_id;
    
    m_interp_factor = t;
}

//-----------------------------------------------------------------------------
// CoopPlayerRegistry
//-----------------------------------------------------------------------------

static CoopPlayerRegistry* s_player_registry_instance = nullptr;

CoopPlayerRegistry& CoopPlayerRegistry::Instance()
{
    if (!s_player_registry_instance)
    {
        s_player_registry_instance = new CoopPlayerRegistry();
    }
    return *s_player_registry_instance;
}

CoopPlayerRegistry::CoopPlayerRegistry()
    : m_local_player_net_id(COOP_NETID_INVALID)
{
}

CoopPlayerRegistry::~CoopPlayerRegistry()
{
    Clear();
}

void CoopPlayerRegistry::RegisterPlayer(CoopPlayerState* player)
{
    if (!player || player->GetCoopNetID() == COOP_NETID_INVALID)
        return;
    
    m_players[player->GetCoopNetID()] = player;
}

void CoopPlayerRegistry::UnregisterPlayer(CoopPlayerState* player)
{
    if (!player)
        return;
    
    UnregisterPlayer(player->GetCoopNetID());
}

void CoopPlayerRegistry::UnregisterPlayer(u32 net_id)
{
    auto it = m_players.find(net_id);
    if (it != m_players.end())
    {
        m_players.erase(it);
    }
}

CoopPlayerState* CoopPlayerRegistry::GetPlayer(u32 net_id) const
{
    auto it = m_players.find(net_id);
    return (it != m_players.end()) ? it->second : nullptr;
}

CoopPlayerState* CoopPlayerRegistry::GetLocalPlayer() const
{
    return GetPlayer(m_local_player_net_id);
}

void CoopPlayerRegistry::UpdateRemotePlayers(u32 render_time)
{
    for (auto& pair : m_players)
    {
        if (pair.second && pair.first != m_local_player_net_id)
        {
            pair.second->InterpolateState(render_time);
            pair.second->ApplyToActor();
        }
    }
}

void CoopPlayerRegistry::Clear()
{
    m_players.clear();
}

} // namespace coop
