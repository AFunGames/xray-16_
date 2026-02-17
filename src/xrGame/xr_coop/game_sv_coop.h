#pragma once
// game_sv_coop.h: Server-side co-op game state
//

#include "game_sv_single.h"
#include "xr_coop/coop_protocol.h"

class NET_Packet;

namespace coop
{
class CoopPlayerState;
}

/**
 * Server-side game state for cooperative multiplayer.
 * Extends single-player game state to support multiple players.
 * The server owns the world simulation and broadcasts state to clients.
 */
class game_sv_Coop : public game_sv_Single
{
    typedef game_sv_Single inherited;
    
public:
    game_sv_Coop();
    virtual ~game_sv_Coop();
    
    // Game type identification
    virtual LPCSTR type_name() const override { return "coop"; }
    
    // Initialization
    virtual void Create(shared_str& options) override;
    
    // Main update loop
    virtual void Update() override;
    
    // Player management
    virtual void OnPlayerConnect(ClientID id_who) override;
    virtual void OnPlayerDisconnect(ClientID id_who, pstr Name, u16 GameID) override;
    virtual void OnPlayerReady(ClientID id_who) override;
    virtual void OnPlayerEnteredGame(ClientID id_who) override;
    
    // Entity events
    virtual void OnCreate(u16 id_who) override;
    virtual BOOL OnTouch(u16 eid_who, u16 eid_target, BOOL bForced = FALSE) override;
    virtual void OnDetach(u16 eid_who, u16 eid_target) override;
    
    // Damage handling
    virtual void OnHit(u16 id_hitter, u16 id_hitted, NET_Packet& P) override;
    
    // State export
    virtual void net_Export_State(NET_Packet& P, ClientID id_to) override;
    virtual void net_Export_Update(NET_Packet& P, ClientID id_to, ClientID id) override;
    
    // Level transitions
    virtual bool change_level(NET_Packet& net_packet, ClientID sender) override;
    virtual shared_str level_name(const shared_str& server_options) const override;
    
    // Co-op specific
    bool IsCoopSession() const { return true; }
    u32 GetMaxPlayers() const { return m_max_players; }
    
    // Spawn player for client
    void SpawnPlayerForClient(ClientID client_id);
    void DespawnPlayerForClient(ClientID client_id);
    
    // Sync AI to clients
    void SyncAIToClients();
    
protected:
    // Send state snapshots to all clients
    void SendPlayerSnapshots();
    void SendEntitySnapshots();
    void SendAISnapshots();
    
    // Handle co-op specific messages
    void ProcessCoopMessage(NET_Packet& P, ClientID sender);
    
private:
    u32 m_max_players;
    u32 m_last_sync_time;
    u32 m_snapshot_id;
    
    // Player states mapped by ClientID
    using PlayerStateMap = xr_map<ClientID, coop::CoopPlayerState*>;
    PlayerStateMap m_player_states;
    
    // Spawn management
    struct PendingSpawn
    {
        ClientID client_id;
        u32 spawn_time;
    };
    xr_vector<PendingSpawn> m_pending_spawns;
};
