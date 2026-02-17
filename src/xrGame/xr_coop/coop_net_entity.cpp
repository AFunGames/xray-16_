// coop_net_entity.cpp: Implementation of co-op network entity management
//

#include "StdAfx.h"
#include "coop_net_entity.h"

namespace coop
{

//-----------------------------------------------------------------------------
// CoopEntityRegistry
//-----------------------------------------------------------------------------

static CoopEntityRegistry* s_registry_instance = nullptr;

CoopEntityRegistry& CoopEntityRegistry::Instance()
{
    if (!s_registry_instance)
    {
        s_registry_instance = new CoopEntityRegistry();
    }
    return *s_registry_instance;
}

CoopEntityRegistry::CoopEntityRegistry()
    : m_next_player_id(COOP_NETID_PLAYER_MIN)
    , m_next_entity_id(COOP_NETID_ENTITY_MIN)
{
}

CoopEntityRegistry::~CoopEntityRegistry()
{
    Clear();
}

void CoopEntityRegistry::RegisterEntity(ICoopNetEntity* entity)
{
    if (!entity || entity->GetCoopNetID() == COOP_NETID_INVALID)
        return;
    
    m_lock.Enter();
    m_entities[entity->GetCoopNetID()] = entity;
    m_lock.Leave();
}

void CoopEntityRegistry::UnregisterEntity(ICoopNetEntity* entity)
{
    if (!entity)
        return;
    
    UnregisterEntity(entity->GetCoopNetID());
}

void CoopEntityRegistry::UnregisterEntity(u32 net_id)
{
    if (net_id == COOP_NETID_INVALID)
        return;
    
    m_lock.Enter();
    auto it = m_entities.find(net_id);
    if (it != m_entities.end())
    {
        m_entities.erase(it);
    }
    m_lock.Leave();
}

ICoopNetEntity* CoopEntityRegistry::GetEntity(u32 net_id) const
{
    if (net_id == COOP_NETID_INVALID)
        return nullptr;
    
    auto it = m_entities.find(net_id);
    return (it != m_entities.end()) ? it->second : nullptr;
}

bool CoopEntityRegistry::HasEntity(u32 net_id) const
{
    return m_entities.find(net_id) != m_entities.end();
}

u32 CoopEntityRegistry::AllocateNetID(bool is_player)
{
    m_lock.Enter();
    u32 id = COOP_NETID_INVALID;
    
    if (is_player)
    {
        if (m_next_player_id <= COOP_NETID_PLAYER_MAX)
        {
            id = m_next_player_id++;
        }
    }
    else
    {
        id = m_next_entity_id++;
    }
    
    m_lock.Leave();
    return id;
}

void CoopEntityRegistry::FreeNetID(u32 net_id)
{
    // For now, we don't recycle IDs
    // Could implement a free list if needed
}

void CoopEntityRegistry::GetDirtyEntities(xr_vector<ICoopNetEntity*>& out_dirty) const
{
    out_dirty.clear();
    
    for (auto& pair : m_entities)
    {
        if (pair.second && pair.second->IsCoopDirty())
        {
            out_dirty.push_back(pair.second);
        }
    }
}

void CoopEntityRegistry::Clear()
{
    m_lock.Enter();
    m_entities.clear();
    m_next_player_id = COOP_NETID_PLAYER_MIN;
    m_next_entity_id = COOP_NETID_ENTITY_MIN;
    m_lock.Leave();
}

u32 CoopEntityRegistry::GetPlayerCount() const
{
    u32 count = 0;
    for (auto& pair : m_entities)
    {
        if (IsPlayerNetID(pair.first))
        {
            count++;
        }
    }
    return count;
}

//-----------------------------------------------------------------------------
// CoopNetEntityBase
//-----------------------------------------------------------------------------

CoopNetEntityBase::CoopNetEntityBase()
    : m_coop_net_id(COOP_NETID_INVALID)
    , m_coop_dirty(false)
    , m_coop_locally_owned(false)
{
}

CoopNetEntityBase::~CoopNetEntityBase()
{
    // Unregister from registry if registered
    if (m_coop_net_id != COOP_NETID_INVALID)
    {
        CoopEntityRegistry::Instance().UnregisterEntity(this);
    }
}

float CoopNetEntityBase::GetCoopReplicationPriority(const Fvector& viewerPos) const
{
    // Default priority based on distance
    // Closer entities have higher priority
    Fvector myPos = GetCoopPosition();
    float dist_sq = viewerPos.distance_to_sqr(myPos);
    
    // Clamp and invert: close = high priority
    const float max_dist_sq = 100.0f * 100.0f;  // 100 meters
    if (dist_sq >= max_dist_sq)
        return 0.0f;
    
    return 1.0f - (dist_sq / max_dist_sq);
}

} // namespace coop
