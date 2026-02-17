#pragma once
// game_cl_coop.h: Client-side co-op game state
//

#include "game_cl_single.h"
#include "xr_coop/coop_protocol.h"

class NET_Packet;

namespace coop
{
class CoopPlayerState;
}

/**
 * Client-side game state for cooperative multiplayer.
 * Extends single-player game state for co-op experience.
 * Receives state from server and renders remote players.
 */
class game_cl_Coop : public game_cl_Single
{
    typedef game_cl_Single inherited;
    
public:
    game_cl_Coop();
    virtual ~game_cl_Coop();
    
    // Game type identification
    virtual pcstr getTeamSection(int Team) override;
    
    // UI creation
    virtual CUIGameCustom* createGameUI() override;
    
    // Server control
    virtual bool IsServerControlHits() override { return true; }  // Server validates hits
    
    // Network message handling
    void OnCoopMessage(NET_Packet& P, u16 msg_type);
    
    // Frame update
    void Update();
    
    // Rendering
    void OnRender();
    
    // Input handling
    void SendPlayerInput();
    void OnFireWeapon();
    
    // Player state
    coop::CoopPlayerState* GetLocalPlayerState() const { return m_local_player_state; }
    
    // Remote player management
    void OnRemotePlayerSpawn(NET_Packet& P);
    void OnRemotePlayerDespawn(NET_Packet& P);
    void OnRemotePlayerState(NET_Packet& P);
    
    // AI proxy management  
    void OnAISpawn(NET_Packet& P);
    void OnAIDespawn(NET_Packet& P);
    void OnAIState(NET_Packet& P);
    
protected:
    // Process different message types
    void ProcessPlayerInput();
    void InterpolateRemotePlayers();
    void InterpolateAI();
    
private:
    coop::CoopPlayerState* m_local_player_state;
    u32 m_last_input_time;
    u32 m_input_sequence;
    
    // Remote players
    using RemotePlayerMap = xr_map<u32, coop::CoopPlayerState*>;
    RemotePlayerMap m_remote_players;
};
