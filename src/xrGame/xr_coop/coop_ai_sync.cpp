// coop_ai_sync.cpp: Implementation of AI entity synchronization
//

#include "StdAfx.h"
#include "coop_ai_sync.h"
#include "coop_session_manager.h"
#include "Entity.h"

namespace coop
{

//-----------------------------------------------------------------------------
// CoopAIEntity
//-----------------------------------------------------------------------------

CoopAIEntity::CoopAIEntity()
    : m_entity(nullptr)
    , m_interp_buffer_head(0)
    , m_interp_buffer_count(0)
    , m_last_update_time(0)
{
    ZeroMemory(&m_state, sizeof(m_state));
    ZeroMemory(m_interp_buffer, sizeof(m_interp_buffer));
}

CoopAIEntity::~CoopAIEntity()
{
    m_entity = nullptr;
}

void CoopAIEntity::BindToEntity(CEntity* entity)
{
    m_entity = entity;
    if (entity)
    {
        MarkCoopDirty();
    }
}

u16 CoopAIEntity::GetCoopEntityType() const
{
    return 0x0002;  // AI entity type
}

Fvector CoopAIEntity::GetCoopPosition() const
{
    if (m_entity)
    {
        return m_entity->Position();
    }
    return Fvector().set(m_state.pos_x, m_state.pos_y, m_state.pos_z);
}

void CoopAIEntity::WriteCoopSnapshot(NET_Packet& P) const
{
    P.w_u32(m_state.net_id);
    P.w_u32(m_state.timestamp);
    
    P.w_float(m_state.pos_x);
    P.w_float(m_state.pos_y);
    P.w_float(m_state.pos_z);
    P.w_float(m_state.yaw);
    P.w_float(m_state.pitch);
    
    P.w_float(m_state.vel_x);
    P.w_float(m_state.vel_y);
    P.w_float(m_state.vel_z);
    
    P.w_float(m_state.health);
    P.w_u16(m_state.anim_state);
    P.w_u8(m_state.ai_state);
    P.w_u8(m_state.flags);
    P.w_u32(m_state.target_net_id);
}

void CoopAIEntity::ReadCoopSnapshot(NET_Packet& P)
{
    CoopAIState new_state;
    
    new_state.net_id = P.r_u32();
    new_state.timestamp = P.r_u32();
    
    new_state.pos_x = P.r_float();
    new_state.pos_y = P.r_float();
    new_state.pos_z = P.r_float();
    new_state.yaw = P.r_float();
    new_state.pitch = P.r_float();
    
    new_state.vel_x = P.r_float();
    new_state.vel_y = P.r_float();
    new_state.vel_z = P.r_float();
    
    new_state.health = P.r_float();
    new_state.anim_state = P.r_u16();
    new_state.ai_state = P.r_u8();
    new_state.flags = P.r_u8();
    new_state.target_net_id = P.r_u32();
    
    // Add to interpolation buffer if this is a remote entity
    if (!IsCoopLocallyOwned())
    {
        AddStateToBuffer(new_state);
    }
    
    m_state = new_state;
    m_last_update_time = Device.dwTimeGlobal;
}

void CoopAIEntity::UpdateFromEntity()
{
    if (!m_entity)
        return;
    
    m_state.net_id = GetCoopNetID();
    m_state.timestamp = Device.dwTimeGlobal;
    
    // Position
    const Fvector& pos = m_entity->Position();
    m_state.pos_x = pos.x;
    m_state.pos_y = pos.y;
    m_state.pos_z = pos.z;
    
    // Rotation
    const Fvector& dir = m_entity->Direction();
    m_state.yaw = dir.getH();
    m_state.pitch = dir.getP();
    
    // Velocity (if available)
    m_state.vel_x = 0;
    m_state.vel_y = 0;
    m_state.vel_z = 0;
    
    // Health
    m_state.health = m_entity->GetfHealth();
    
    // AI state (simplified)
    if (m_state.health <= 0)
    {
        m_state.ai_state = 4;  // Dead
    }
    else
    {
        m_state.ai_state = 0;  // Idle (would need AI brain access for real state)
    }
    
    m_last_update_time = Device.dwTimeGlobal;
    MarkCoopDirty();
}

void CoopAIEntity::ApplyToEntity()
{
    if (!m_entity || IsCoopLocallyOwned())
        return;
    
    // Apply position
    Fvector pos;
    pos.set(m_state.pos_x, m_state.pos_y, m_state.pos_z);
    
    // Only apply if reasonable distance (prevent teleporting artifacts)
    float dist = m_entity->Position().distance_to(pos);
    if (dist < 20.0f)
    {
        m_entity->Position() = pos;
    }
    
    // Apply rotation
    Fvector dir;
    dir.setHP(m_state.yaw, m_state.pitch);
    m_entity->Direction() = dir;
}

void CoopAIEntity::AddStateToBuffer(const CoopAIState& state)
{
    m_interp_buffer[m_interp_buffer_head] = state;
    m_interp_buffer_head = (m_interp_buffer_head + 1) % INTERP_BUFFER_SIZE;
    
    if (m_interp_buffer_count < INTERP_BUFFER_SIZE)
    {
        m_interp_buffer_count++;
    }
}

void CoopAIEntity::InterpolateState(u32 render_time)
{
    if (m_interp_buffer_count < 2)
        return;
    
    // Find two states to interpolate between
    u32 interp_delay = 150;  // 150ms delay for AI
    u32 target_time = render_time - interp_delay;
    
    int from_idx = -1;
    int to_idx = -1;
    
    for (u32 i = 0; i < m_interp_buffer_count; i++)
    {
        u32 idx = (m_interp_buffer_head - 1 - i + INTERP_BUFFER_SIZE) % INTERP_BUFFER_SIZE;
        
        if (m_interp_buffer[idx].timestamp <= target_time)
        {
            from_idx = idx;
            if (i > 0)
            {
                to_idx = (idx + 1) % INTERP_BUFFER_SIZE;
            }
            break;
        }
    }
    
    if (from_idx < 0 || to_idx < 0)
    {
        m_state = m_interp_buffer[(m_interp_buffer_head - 1 + INTERP_BUFFER_SIZE) % INTERP_BUFFER_SIZE];
        return;
    }
    
    const CoopAIState& from = m_interp_buffer[from_idx];
    const CoopAIState& to = m_interp_buffer[to_idx];
    
    float time_diff = static_cast<float>(to.timestamp - from.timestamp);
    if (time_diff <= 0.0f)
    {
        m_state = to;
        return;
    }
    
    float t = static_cast<float>(target_time - from.timestamp) / time_diff;
    t = clamp(t, 0.0f, 1.0f);
    
    // Interpolate position
    m_state.pos_x = from.pos_x + (to.pos_x - from.pos_x) * t;
    m_state.pos_y = from.pos_y + (to.pos_y - from.pos_y) * t;
    m_state.pos_z = from.pos_z + (to.pos_z - from.pos_z) * t;
    
    // Interpolate rotation
    m_state.yaw = from.yaw + (to.yaw - from.yaw) * t;
    m_state.pitch = from.pitch + (to.pitch - from.pitch) * t;
    
    // Use latest discrete state
    m_state.health = to.health;
    m_state.anim_state = to.anim_state;
    m_state.ai_state = to.ai_state;
    m_state.flags = to.flags;
    m_state.target_net_id = to.target_net_id;
}

//-----------------------------------------------------------------------------
// CoopAIManager
//-----------------------------------------------------------------------------

static CoopAIManager* s_ai_manager_instance = nullptr;

CoopAIManager& CoopAIManager::Instance()
{
    if (!s_ai_manager_instance)
    {
        s_ai_manager_instance = new CoopAIManager();
    }
    return *s_ai_manager_instance;
}

CoopAIManager::CoopAIManager()
    : m_last_update_time(0)
    , m_update_interval(100)  // 10 Hz
{
}

CoopAIManager::~CoopAIManager()
{
    Shutdown();
}

void CoopAIManager::Initialize()
{
    Msg("[COOP] AI manager initialized");
}

void CoopAIManager::Shutdown()
{
    Clear();
    Msg("[COOP] AI manager shutdown");
}

void CoopAIManager::RegisterAI(CEntity* entity)
{
    if (!entity)
        return;
    
    // Allocate network ID for this AI
    u32 net_id = CoopEntityRegistry::Instance().AllocateNetID(false);
    if (net_id == COOP_NETID_INVALID)
    {
        Msg("![COOP] Failed to allocate net ID for AI entity");
        return;
    }
    
    // Create AI entity wrapper
    CoopAIEntity* ai_entity = new CoopAIEntity();
    ai_entity->SetCoopNetID(net_id);
    ai_entity->BindToEntity(entity);
    ai_entity->SetCoopLocallyOwned(CoopSessionManager::Instance().IsHost());
    
    m_ai_entities[net_id] = ai_entity;
    CoopEntityRegistry::Instance().RegisterEntity(ai_entity);
    
    Msg("[COOP] Registered AI entity with net_id=%u", net_id);
}

void CoopAIManager::UnregisterAI(CEntity* entity)
{
    if (!entity)
        return;
    
    // Find by entity pointer
    for (auto it = m_ai_entities.begin(); it != m_ai_entities.end(); ++it)
    {
        if (it->second && it->second->GetEntity() == entity)
        {
            UnregisterAI(it->first);
            return;
        }
    }
}

void CoopAIManager::UnregisterAI(u32 net_id)
{
    auto it = m_ai_entities.find(net_id);
    if (it != m_ai_entities.end())
    {
        CoopEntityRegistry::Instance().UnregisterEntity(net_id);
        delete it->second;
        m_ai_entities.erase(it);
        
        Msg("[COOP] Unregistered AI entity with net_id=%u", net_id);
    }
}

CoopAIEntity* CoopAIManager::GetAI(u32 net_id) const
{
    auto it = m_ai_entities.find(net_id);
    return (it != m_ai_entities.end()) ? it->second : nullptr;
}

void CoopAIManager::UpdateServerAI()
{
    if (!CoopSessionManager::Instance().IsHost())
        return;
    
    u32 current_time = Device.dwTimeGlobal;
    
    // Update at fixed rate
    if (current_time - m_last_update_time < m_update_interval)
        return;
    
    m_last_update_time = current_time;
    
    // Update all AI entities
    for (auto& pair : m_ai_entities)
    {
        if (pair.second)
        {
            pair.second->UpdateFromEntity();
        }
    }
}

void CoopAIManager::UpdateClientAI(u32 render_time)
{
    if (CoopSessionManager::Instance().IsHost())
        return;
    
    // Update all AI proxies
    for (auto& pair : m_ai_entities)
    {
        if (pair.second)
        {
            pair.second->InterpolateState(render_time);
            pair.second->ApplyToEntity();
        }
    }
}

void CoopAIManager::CreateSnapshots(xr_vector<NET_Packet>& out_packets)
{
    for (auto& pair : m_ai_entities)
    {
        if (pair.second && pair.second->IsCoopDirty())
        {
            NET_Packet P;
            P.w_begin(M_COOP_AI_STATE);
            pair.second->WriteCoopSnapshot(P);
            out_packets.push_back(P);
            
            pair.second->ClearCoopDirty();
        }
    }
}

void CoopAIManager::ApplySnapshot(NET_Packet& P)
{
    // Read net_id from packet (peek without advancing)
    u32 net_id = P.r_u32();
    P.r_seek(P.r_tell() - sizeof(u32));  // Seek back
    
    CoopAIEntity* ai = GetAI(net_id);
    if (ai)
    {
        ai->ReadCoopSnapshot(P);
    }
    else
    {
        Msg("[COOP] Received AI snapshot for unknown entity: net_id=%u", net_id);
    }
}

u32 CoopAIManager::GetDirtyAICount() const
{
    u32 count = 0;
    for (const auto& pair : m_ai_entities)
    {
        if (pair.second && pair.second->IsCoopDirty())
        {
            count++;
        }
    }
    return count;
}

void CoopAIManager::Clear()
{
    for (auto& pair : m_ai_entities)
    {
        if (pair.second)
        {
            CoopEntityRegistry::Instance().UnregisterEntity(pair.first);
            delete pair.second;
        }
    }
    m_ai_entities.clear();
}

} // namespace coop
