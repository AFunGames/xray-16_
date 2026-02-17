#pragma once
// coop_ai_sync.h: Server-owned AI entity synchronization
//

#include "xrCore/xrCore.h"
#include "coop_protocol.h"
#include "coop_net_entity.h"

class NET_Packet;
class CEntity;

namespace coop
{

/**
 * State information for a synchronized AI NPC.
 */
struct CoopAIState
{
    u32 net_id;
    u32 timestamp;
    
    // Position and rotation
    float pos_x, pos_y, pos_z;
    float yaw, pitch;
    
    // Velocity
    float vel_x, vel_y, vel_z;
    
    // State
    float health;
    u16 anim_state;
    u8 ai_state;  // 0=idle, 1=patrol, 2=alert, 3=combat, 4=dead
    u8 flags;
    
    // Target (if in combat)
    u32 target_net_id;
    
    CoopAIState()
        : net_id(COOP_NETID_INVALID), timestamp(0)
        , pos_x(0), pos_y(0), pos_z(0)
        , yaw(0), pitch(0)
        , vel_x(0), vel_y(0), vel_z(0)
        , health(1.0f), anim_state(0), ai_state(0), flags(0)
        , target_net_id(COOP_NETID_INVALID) {}
};

/**
 * Wrapper for AI entities that need network synchronization.
 * On server: captures entity state and sends to clients.
 * On client: receives state and applies to visual proxy.
 */
class CoopAIEntity : public CoopNetEntityBase
{
public:
    CoopAIEntity();
    virtual ~CoopAIEntity();
    
    // Associate with game entity
    void BindToEntity(CEntity* entity);
    CEntity* GetEntity() const { return m_entity; }
    
    // ICoopNetEntity implementation
    virtual void WriteCoopSnapshot(NET_Packet& P) const override;
    virtual void ReadCoopSnapshot(NET_Packet& P) override;
    virtual u16 GetCoopEntityType() const override;
    virtual Fvector GetCoopPosition() const override;
    
    // State accessors
    const CoopAIState& GetState() const { return m_state; }
    CoopAIState& GetStateMutable() { return m_state; }
    
    // Update from local entity (server only)
    void UpdateFromEntity();
    
    // Apply state to visual proxy (client only)
    void ApplyToEntity();
    
    // Interpolation for smooth movement
    void AddStateToBuffer(const CoopAIState& state);
    void InterpolateState(u32 render_time);
    
private:
    CEntity* m_entity;
    CoopAIState m_state;
    
    // Interpolation buffer
    static const u32 INTERP_BUFFER_SIZE = 3;
    CoopAIState m_interp_buffer[INTERP_BUFFER_SIZE];
    u32 m_interp_buffer_head;
    u32 m_interp_buffer_count;
    
    u32 m_last_update_time;
};

/**
 * Manages AI synchronization for co-op mode.
 * Server owns all AI simulation; clients receive visual updates.
 */
class CoopAIManager
{
public:
    static CoopAIManager& Instance();
    
    // Initialization
    void Initialize();
    void Shutdown();
    
    // Register/unregister AI entities
    void RegisterAI(CEntity* entity);
    void UnregisterAI(CEntity* entity);
    void UnregisterAI(u32 net_id);
    
    // Lookup
    CoopAIEntity* GetAI(u32 net_id) const;
    
    // Update all AI states (server only, called each tick)
    void UpdateServerAI();
    
    // Update all AI proxies (client only, called each frame)
    void UpdateClientAI(u32 render_time);
    
    // Create snapshots for all dirty AI entities
    void CreateSnapshots(xr_vector<NET_Packet>& out_packets);
    
    // Apply received snapshot
    void ApplySnapshot(NET_Packet& P);
    
    // Get statistics
    u32 GetAICount() const { return static_cast<u32>(m_ai_entities.size()); }
    u32 GetDirtyAICount() const;
    
    // Clear all AI (level unload)
    void Clear();
    
private:
    CoopAIManager();
    ~CoopAIManager();
    
    using AIMap = xr_map<u32, CoopAIEntity*>;
    AIMap m_ai_entities;
    
    u32 m_last_update_time;
    u32 m_update_interval;  // ms between AI updates
};

} // namespace coop
