#pragma once
// coop_net_entity.h: Interface for network-replicable entities
//

#include "xrCore/xrCore.h"
#include "xrCommon/xr_types.h"
#include "coop_protocol.h"

class NET_Packet;

namespace coop
{

// Forward declarations
class CoopInterpolator;

/**
 * Interface for objects that can be replicated over the network in co-op mode.
 * Any entity that needs to be synchronized between host and clients should implement this.
 */
class ICoopNetEntity
{
public:
    virtual ~ICoopNetEntity() = default;
    
    // Network ID management
    virtual u32 GetCoopNetID() const = 0;
    virtual void SetCoopNetID(u32 id) = 0;
    
    // Snapshot serialization
    virtual void WriteCoopSnapshot(NET_Packet& P) const = 0;
    virtual void ReadCoopSnapshot(NET_Packet& P) = 0;
    
    // Dirty tracking for delta compression
    virtual bool IsCoopDirty() const = 0;
    virtual void ClearCoopDirty() = 0;
    virtual void MarkCoopDirty() = 0;
    
    // Replication priority (higher = more important)
    // Used to prioritize updates when bandwidth is limited
    virtual float GetCoopReplicationPriority(const Fvector& viewerPos) const = 0;
    
    // Entity type for deserialization
    virtual u16 GetCoopEntityType() const = 0;
    
    // Is this entity owned by the local player?
    virtual bool IsCoopLocallyOwned() const = 0;
    virtual void SetCoopLocallyOwned(bool owned) = 0;
    
    // Get position for priority calculation
    virtual Fvector GetCoopPosition() const = 0;
};

/**
 * Registry for managing all co-op networked entities.
 * The server maintains the authoritative registry; clients maintain a local cache.
 */
class CoopEntityRegistry
{
public:
    static CoopEntityRegistry& Instance();
    
    // Registration
    void RegisterEntity(ICoopNetEntity* entity);
    void UnregisterEntity(ICoopNetEntity* entity);
    void UnregisterEntity(u32 net_id);
    
    // Lookup
    ICoopNetEntity* GetEntity(u32 net_id) const;
    bool HasEntity(u32 net_id) const;
    
    // ID assignment (server only)
    u32 AllocateNetID(bool is_player = false);
    void FreeNetID(u32 net_id);
    
    // Iteration
    using EntityMap = xr_map<u32, ICoopNetEntity*>;
    const EntityMap& GetAllEntities() const { return m_entities; }
    
    // Get all dirty entities for snapshot creation
    void GetDirtyEntities(xr_vector<ICoopNetEntity*>& out_dirty) const;
    
    // Clear all entities (level unload)
    void Clear();
    
    // Statistics
    u32 GetEntityCount() const { return static_cast<u32>(m_entities.size()); }
    u32 GetPlayerCount() const;
    
private:
    CoopEntityRegistry();
    ~CoopEntityRegistry();
    
    CoopEntityRegistry(const CoopEntityRegistry&) = delete;
    CoopEntityRegistry& operator=(const CoopEntityRegistry&) = delete;
    
    EntityMap m_entities;
    u32 m_next_player_id;
    u32 m_next_entity_id;
    
    Lock m_lock;  // Thread safety
};

/**
 * Base implementation of ICoopNetEntity with common functionality.
 * Game objects can inherit from this instead of implementing the interface directly.
 */
class CoopNetEntityBase : public ICoopNetEntity
{
public:
    CoopNetEntityBase();
    virtual ~CoopNetEntityBase();
    
    // ICoopNetEntity implementation
    virtual u32 GetCoopNetID() const override { return m_coop_net_id; }
    virtual void SetCoopNetID(u32 id) override { m_coop_net_id = id; }
    
    virtual bool IsCoopDirty() const override { return m_coop_dirty; }
    virtual void ClearCoopDirty() override { m_coop_dirty = false; }
    virtual void MarkCoopDirty() override { m_coop_dirty = true; }
    
    virtual bool IsCoopLocallyOwned() const override { return m_coop_locally_owned; }
    virtual void SetCoopLocallyOwned(bool owned) override { m_coop_locally_owned = owned; }
    
    // Default priority based on distance
    virtual float GetCoopReplicationPriority(const Fvector& viewerPos) const override;
    
protected:
    u32 m_coop_net_id;
    bool m_coop_dirty;
    bool m_coop_locally_owned;
    
    // Last replicated state for dirty detection
    mutable CoopEntitySnapshot m_last_snapshot;
};

} // namespace coop
