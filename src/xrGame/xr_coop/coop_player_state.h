#pragma once
// coop_player_state.h: Player state management for co-op replication
//

#include "xrCore/xrCore.h"
#include "coop_protocol.h"
#include "coop_net_entity.h"

class NET_Packet;
class CActor;

namespace coop
{

/**
 * Manages the network state of a player in co-op mode.
 * Local players send their state to the server.
 * Remote players receive state from the server and interpolate.
 */
class CoopPlayerState : public CoopNetEntityBase
{
public:
    CoopPlayerState();
    virtual ~CoopPlayerState();
    
    // Associate with game actor
    void BindToActor(CActor* actor);
    CActor* GetActor() const { return m_actor; }
    
    // ICoopNetEntity implementation
    virtual void WriteCoopSnapshot(NET_Packet& P) const override;
    virtual void ReadCoopSnapshot(NET_Packet& P) override;
    virtual u16 GetCoopEntityType() const override;
    virtual Fvector GetCoopPosition() const override;
    
    // State accessors
    const CoopPlayerSnapshot& GetSnapshot() const { return m_snapshot; }
    CoopPlayerSnapshot& GetSnapshotMutable() { return m_snapshot; }
    
    // Update from local actor (for local player sending state)
    void UpdateFromActor();
    
    // Apply state to actor (for remote players receiving state)
    void ApplyToActor();
    
    // Input handling
    void SetInput(const CoopPlayerInput& input);
    const CoopPlayerInput& GetInput() const { return m_input; }
    
    // Interpolation
    void AddSnapshotToBuffer(const CoopPlayerSnapshot& snapshot);
    void InterpolateState(u32 render_time);
    
    // Player info
    const xr_string& GetPlayerName() const { return m_player_name; }
    void SetPlayerName(const char* name) { m_player_name = name; }
    
    u16 GetPlayerFlags() const { return m_snapshot.player_flags; }
    bool IsAlive() const { return (m_snapshot.player_flags & COOP_PLAYER_FLAG_ALIVE) != 0; }
    bool IsDowned() const { return (m_snapshot.player_flags & COOP_PLAYER_FLAG_DOWNED) != 0; }
    
    // Statistics
    u32 GetLastUpdateTime() const { return m_last_update_time; }
    
private:
    CActor* m_actor;
    xr_string m_player_name;
    
    CoopPlayerSnapshot m_snapshot;
    CoopPlayerInput m_input;
    
    // Interpolation buffer
    static const u32 INTERP_BUFFER_SIZE = COOP_INTERP_BUFFER_SIZE;
    CoopPlayerSnapshot m_interp_buffer[INTERP_BUFFER_SIZE];
    u32 m_interp_buffer_head;
    u32 m_interp_buffer_count;
    
    // Interpolation state
    CoopPlayerSnapshot m_interp_from;
    CoopPlayerSnapshot m_interp_to;
    float m_interp_factor;
    
    u32 m_last_update_time;
    SnapshotID m_last_snapshot_id;
};

/**
 * Registry for all player states in the current session.
 */
class CoopPlayerRegistry
{
public:
    static CoopPlayerRegistry& Instance();
    
    // Registration
    void RegisterPlayer(CoopPlayerState* player);
    void UnregisterPlayer(CoopPlayerState* player);
    void UnregisterPlayer(u32 net_id);
    
    // Lookup
    CoopPlayerState* GetPlayer(u32 net_id) const;
    CoopPlayerState* GetLocalPlayer() const;
    
    // Iteration
    using PlayerMap = xr_map<u32, CoopPlayerState*>;
    const PlayerMap& GetAllPlayers() const { return m_players; }
    
    // Update all remote players (interpolation)
    void UpdateRemotePlayers(u32 render_time);
    
    // Clear all players (level unload)
    void Clear();
    
    // Set local player ID
    void SetLocalPlayerNetID(u32 net_id) { m_local_player_net_id = net_id; }
    u32 GetLocalPlayerNetID() const { return m_local_player_net_id; }
    
private:
    CoopPlayerRegistry();
    ~CoopPlayerRegistry();
    
    PlayerMap m_players;
    u32 m_local_player_net_id;
};

} // namespace coop
