// game_sv_coop.cpp: Server-side co-op game state implementation
//

#include "StdAfx.h"
#include "game_sv_coop.h"
#include "coop_session_manager.h"
#include "coop_player_state.h"
#include "coop_ai_sync.h"
#include "coop_net_entity.h"
#include "coop_debug_overlay.h"
#include "xrServer.h"
#include "Level.h"
#include "Actor.h"
#include "xrServerEntities/xrMessages.h"

game_sv_Coop::game_sv_Coop()
    : m_max_players(coop::COOP_MAX_PLAYERS)
    , m_last_sync_time(0)
    , m_snapshot_id(0)
{
    m_type = eGameIDCoop;  // Set proper game type for co-op mode
}

game_sv_Coop::~game_sv_Coop()
{
    // Clean up player states
    for (auto& pair : m_player_states)
    {
        delete pair.second;
    }
    m_player_states.clear();
}

void game_sv_Coop::Create(shared_str& options)
{
    inherited::Create(options);
    
    // Initialize co-op systems
    coop::CoopSessionManager::Instance().Initialize();
    coop::CoopAIManager::Instance().Initialize();
    
    // Parse co-op options
    m_max_players = get_option_i(*options, "maxplayers", coop::COOP_MAX_PLAYERS);
    if (m_max_players > coop::COOP_MAX_PLAYERS)
        m_max_players = coop::COOP_MAX_PLAYERS;
    
    Msg("[COOP] Server game created, max players: %u", m_max_players);
}

void game_sv_Coop::Update()
{
    inherited::Update();
    
    u32 current_time = Device.dwTimeGlobal;
    
    // Update co-op session
    coop::CoopSessionManager::Instance().Update();
    
    // Update AI on server
    coop::CoopAIManager::Instance().UpdateServerAI();
    
    // Send state snapshots at fixed rate
    const u32 sync_interval = 50;  // 20 Hz
    if (current_time - m_last_sync_time >= sync_interval)
    {
        m_last_sync_time = current_time;
        
        SendPlayerSnapshots();
        SendAISnapshots();
    }
    
    // Process pending spawns
    for (auto it = m_pending_spawns.begin(); it != m_pending_spawns.end();)
    {
        if (current_time >= it->spawn_time)
        {
            SpawnPlayerForClient(it->client_id);
            it = m_pending_spawns.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void game_sv_Coop::OnPlayerConnect(ClientID id_who)
{
    inherited::OnPlayerConnect(id_who);
    
    Msg("[COOP] Player connected: ClientID=%u", id_who.value());
    
    // Create player state
    coop::CoopPlayerState* player_state = new coop::CoopPlayerState();
    u32 net_id = coop::CoopEntityRegistry::Instance().AllocateNetID(true);
    player_state->SetCoopNetID(net_id);
    player_state->SetCoopLocallyOwned(false);  // Server owns all state
    
    m_player_states[id_who] = player_state;
    coop::CoopEntityRegistry::Instance().RegisterEntity(player_state);
    coop::CoopPlayerRegistry::Instance().RegisterPlayer(player_state);
    
    // Notify session manager
    coop::CoopPlayer player_info;
    player_info.net_id = net_id;
    player_info.client_id = id_who;
    player_info.name = "Player";
    player_info.flags = 0;
    player_info.join_time = Device.dwTimeGlobal;
    
    coop::CoopSessionManager::Instance().OnPlayerJoined(player_info);
    
    // Schedule spawn after a short delay
    PendingSpawn spawn;
    spawn.client_id = id_who;
    spawn.spawn_time = Device.dwTimeGlobal + 500;  // 500ms delay
    m_pending_spawns.push_back(spawn);
}

void game_sv_Coop::OnPlayerDisconnect(ClientID id_who, pstr Name, u16 GameID)
{
    Msg("[COOP] Player disconnected: ClientID=%u, Name=%s", id_who.value(), Name);
    
    // Clean up player state
    auto it = m_player_states.find(id_who);
    if (it != m_player_states.end())
    {
        u32 net_id = it->second->GetCoopNetID();
        
        coop::CoopPlayerRegistry::Instance().UnregisterPlayer(it->second);
        coop::CoopEntityRegistry::Instance().UnregisterEntity(it->second);
        coop::CoopSessionManager::Instance().OnPlayerLeft(net_id);
        
        // Despawn the player actor
        DespawnPlayerForClient(id_who);
        
        delete it->second;
        m_player_states.erase(it);
    }
    
    // Remove from pending spawns
    for (auto pit = m_pending_spawns.begin(); pit != m_pending_spawns.end();)
    {
        if (pit->client_id == id_who)
        {
            pit = m_pending_spawns.erase(pit);
        }
        else
        {
            ++pit;
        }
    }
    
    inherited::OnPlayerDisconnect(id_who, Name, GameID);
}

void game_sv_Coop::OnPlayerReady(ClientID id_who)
{
    inherited::OnPlayerReady(id_who);
    
    auto it = m_player_states.find(id_who);
    if (it != m_player_states.end())
    {
        coop::CoopSessionManager::Instance().OnPlayerReady(it->second->GetCoopNetID());
    }
}

void game_sv_Coop::OnPlayerEnteredGame(ClientID id_who)
{
    inherited::OnPlayerEnteredGame(id_who);
    
    Msg("[COOP] Player entered game: ClientID=%u", id_who.value());
}

void game_sv_Coop::OnCreate(u16 id_who)
{
    inherited::OnCreate(id_who);
}

BOOL game_sv_Coop::OnTouch(u16 eid_who, u16 eid_target, BOOL bForced)
{
    return inherited::OnTouch(eid_who, eid_target, bForced);
}

void game_sv_Coop::OnDetach(u16 eid_who, u16 eid_target)
{
    inherited::OnDetach(eid_who, eid_target);
}

void game_sv_Coop::OnHit(u16 id_hitter, u16 id_hitted, NET_Packet& P)
{
    inherited::OnHit(id_hitter, id_hitted, P);
    
    // Mark involved entities as dirty for sync
    // Find entities in co-op registry and mark dirty
    // This will trigger state sync on next update
}

void game_sv_Coop::net_Export_State(NET_Packet& P, ClientID id_to)
{
    inherited::net_Export_State(P, id_to);
    
    // Add co-op session info
    const auto& session = coop::CoopSessionManager::Instance();
    P.w_u32(coop::COOP_PROTOCOL_VERSION);
    P.w_u32(session.GetPlayerCount());
}

void game_sv_Coop::net_Export_Update(NET_Packet& P, ClientID id_to, ClientID id)
{
    inherited::net_Export_Update(P, id_to, id);
}

bool game_sv_Coop::change_level(NET_Packet& net_packet, ClientID sender)
{
    // In co-op, level changes should be initiated by host only
    // Verify sender is host (for now, accept all)
    
    Msg("[COOP] Level change requested");
    
    return inherited::change_level(net_packet, sender);
}

shared_str game_sv_Coop::level_name(const shared_str& server_options) const
{
    return inherited::level_name(server_options);
}

void game_sv_Coop::SpawnPlayerForClient(ClientID client_id)
{
    auto it = m_player_states.find(client_id);
    if (it == m_player_states.end())
    {
        Msg("![COOP] Cannot spawn player: unknown ClientID=%u", client_id.value());
        return;
    }
    
    coop::CoopPlayerState* player_state = it->second;
    
    // Find a spawn point
    // For now, use the same spawn point as single-player actor
    RPoint spawn_point = getRP(0, 0);  // Team 0, point 0
    
    Msg("[COOP] Spawning player at (%.1f, %.1f, %.1f)", 
        spawn_point.P.x, spawn_point.P.y, spawn_point.P.z);
    
    // Create actor entity for this player
    // This would typically use the server's entity creation system
    // For MVP, we send a spawn message to the client
    
    NET_Packet P;
    P.w_begin(M_COOP_PLAYER_SPAWN);
    P.w_u32(player_state->GetCoopNetID());
    P.w_float(spawn_point.P.x);
    P.w_float(spawn_point.P.y);
    P.w_float(spawn_point.P.z);
    P.w_float(spawn_point.A.y);  // Yaw
    
    // Send to specific client
    // TODO: Use server's send function
}

void game_sv_Coop::DespawnPlayerForClient(ClientID client_id)
{
    auto it = m_player_states.find(client_id);
    if (it == m_player_states.end())
        return;
    
    coop::CoopPlayerState* player_state = it->second;
    
    // Notify all clients about despawn
    NET_Packet P;
    P.w_begin(M_COOP_PLAYER_DESPAWN);
    P.w_u32(player_state->GetCoopNetID());
    
    // TODO: Broadcast to all clients
}

void game_sv_Coop::SendPlayerSnapshots()
{
    m_snapshot_id++;
    
    for (auto& pair : m_player_states)
    {
        coop::CoopPlayerState* state = pair.second;
        if (state && state->IsCoopDirty())
        {
            NET_Packet P;
            P.w_begin(M_COOP_PLAYER_STATE);
            state->WriteCoopSnapshot(P);
            
            // Broadcast to all clients
            coop::CoopSessionManager::Instance().SendToAll(P, false);
            
            state->ClearCoopDirty();
            
            coop::CoopDebugOverlay::Instance().OnSnapshotSent(m_snapshot_id);
        }
    }
}

void game_sv_Coop::SendAISnapshots()
{
    xr_vector<NET_Packet> packets;
    coop::CoopAIManager::Instance().CreateSnapshots(packets);
    
    for (auto& P : packets)
    {
        coop::CoopSessionManager::Instance().SendToAll(P, false);
    }
}

void game_sv_Coop::SyncAIToClients()
{
    // Force sync of all AI entities
    SendAISnapshots();
}

void game_sv_Coop::ProcessCoopMessage(NET_Packet& P, ClientID sender)
{
    u16 msg_type;
    P.r_begin(msg_type);
    
    switch (msg_type)
    {
    case M_COOP_PLAYER_INPUT:
        {
            // Read input from client
            coop::CoopPlayerInput input;
            input.timestamp = P.r_u32();
            input.move_flags = P.r_u16();
            input.yaw = P.r_float();
            input.pitch = P.r_float();
            input.active_slot = P.r_u8();
            
            // Apply to player state
            auto it = m_player_states.find(sender);
            if (it != m_player_states.end())
            {
                it->second->SetInput(input);
                it->second->MarkCoopDirty();
            }
        }
        break;
        
    case M_COOP_FIRE_EVENT:
        {
            coop::CoopFireEvent fire_event;
            fire_event.timestamp = P.r_u32();
            fire_event.shooter_net_id = P.r_u32();
            fire_event.weapon_id = P.r_u16();
            fire_event.origin_x = P.r_float();
            fire_event.origin_y = P.r_float();
            fire_event.origin_z = P.r_float();
            fire_event.dir_x = P.r_float();
            fire_event.dir_y = P.r_float();
            fire_event.dir_z = P.r_float();
            fire_event.fire_mode = P.r_u8();
            fire_event.ammo_type = P.r_u8();
            
            coop::CoopDamageHandler::Instance().OnFireEvent(fire_event, sender);
        }
        break;
        
    default:
        // Pass to session manager
        coop::CoopSessionManager::Instance().OnMessage(P, msg_type, sender);
        break;
    }
}
