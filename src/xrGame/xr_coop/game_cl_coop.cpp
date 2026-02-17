// game_cl_coop.cpp: Client-side co-op game state implementation
//

#include "StdAfx.h"
#include "game_cl_coop.h"
#include "coop_session_manager.h"
#include "coop_player_state.h"
#include "coop_ai_sync.h"
#include "coop_damage_handler.h"
#include "coop_debug_overlay.h"
#include "coop_net_entity.h"
#include "Level.h"
#include "Actor.h"
#include "Inventory.h"
#include "UIGameSP.h"
#include "xrServerEntities/xrMessages.h"

game_cl_Coop::game_cl_Coop()
    : m_local_player_state(nullptr)
    , m_last_input_time(0)
    , m_input_sequence(0)
{
}

game_cl_Coop::~game_cl_Coop()
{
    // Clean up local player state
    if (m_local_player_state)
    {
        coop::CoopPlayerRegistry::Instance().UnregisterPlayer(m_local_player_state);
        coop::CoopEntityRegistry::Instance().UnregisterEntity(m_local_player_state);
        delete m_local_player_state;
        m_local_player_state = nullptr;
    }
    
    // Clean up remote players
    for (auto& pair : m_remote_players)
    {
        coop::CoopPlayerRegistry::Instance().UnregisterPlayer(pair.second);
        coop::CoopEntityRegistry::Instance().UnregisterEntity(pair.second);
        delete pair.second;
    }
    m_remote_players.clear();
}

pcstr game_cl_Coop::getTeamSection(int Team)
{
    return "coop_team";
}

CUIGameCustom* game_cl_Coop::createGameUI()
{
    // Use single-player UI for now
    // In full implementation, would add co-op specific UI elements
    return inherited::createGameUI();
}

void game_cl_Coop::Update()
{
    u32 current_time = Device.dwTimeGlobal;
    
    // Update session manager
    coop::CoopSessionManager::Instance().Update();
    
    // Update debug overlay
    coop::CoopDebugOverlay::Instance().Update();
    
    // Send player input at fixed rate
    const u32 input_interval = 50;  // 20 Hz
    if (current_time - m_last_input_time >= input_interval)
    {
        m_last_input_time = current_time;
        SendPlayerInput();
    }
    
    // Update local player state from actor
    if (m_local_player_state)
    {
        m_local_player_state->UpdateFromActor();
    }
    
    // Interpolate remote players
    InterpolateRemotePlayers();
    
    // Interpolate AI
    coop::CoopAIManager::Instance().UpdateClientAI(current_time);
}

void game_cl_Coop::OnRender()
{
    // Render debug overlay if enabled
    // This would be called from the HUD render path
}

void game_cl_Coop::SendPlayerInput()
{
    CActor* actor = smart_cast<CActor*>(Level().CurrentControlEntity());
    if (!actor)
        return;
    
    coop::CoopPlayerInput input;
    input.timestamp = Device.dwTimeGlobal;
    
    // Gather movement flags from actor state
    u32 move_state = actor->MovingState();
    input.move_flags = 0;
    
    if (move_state & mcFwd)      input.move_flags |= coop::COOP_MOVE_FWD;
    if (move_state & mcBack)     input.move_flags |= coop::COOP_MOVE_BACK;
    if (move_state & mcLStrafe)  input.move_flags |= coop::COOP_MOVE_LEFT;
    if (move_state & mcRStrafe)  input.move_flags |= coop::COOP_MOVE_RIGHT;
    if (move_state & mcCrouch)   input.move_flags |= coop::COOP_MOVE_CROUCH;
    if (move_state & mcSprint)   input.move_flags |= coop::COOP_MOVE_SPRINT;
    if (move_state & mcJump)     input.move_flags |= coop::COOP_MOVE_JUMP;
    
    // View angles
    input.yaw = actor->r_model_yaw;
    input.pitch = actor->r_torso.pitch;
    
    // Active weapon slot
    input.active_slot = static_cast<u8>(actor->inventory().GetActiveSlot());
    
    // Store locally
    if (m_local_player_state)
    {
        m_local_player_state->SetInput(input);
    }
    
    // Send to server
    NET_Packet P;
    P.w_begin(M_COOP_PLAYER_INPUT);
    P.w_u32(input.timestamp);
    P.w_u16(input.move_flags);
    P.w_float(input.yaw);
    P.w_float(input.pitch);
    P.w_u8(input.active_slot);
    
    coop::CoopSessionManager::Instance().SendToHost(P, false);
    
    m_input_sequence++;
}

void game_cl_Coop::OnFireWeapon()
{
    CActor* actor = smart_cast<CActor*>(Level().CurrentControlEntity());
    if (!actor)
        return;
    
    // Create fire event
    coop::CoopFireEvent fire_event;
    fire_event.timestamp = Device.dwTimeGlobal;
    fire_event.shooter_net_id = m_local_player_state ? m_local_player_state->GetCoopNetID() : 0;
    fire_event.weapon_id = 0;  // Would get from equipped weapon
    
    // Fire origin (weapon muzzle position)
    Fvector origin = actor->Position();
    origin.y += 1.5f;  // Approximate eye height
    fire_event.origin_x = origin.x;
    fire_event.origin_y = origin.y;
    fire_event.origin_z = origin.z;
    
    // Fire direction
    Fvector dir;
    dir.setHP(actor->r_torso.yaw, actor->r_torso.pitch);
    fire_event.dir_x = dir.x;
    fire_event.dir_y = dir.y;
    fire_event.dir_z = dir.z;
    
    fire_event.fire_mode = 0;  // Single shot
    fire_event.ammo_type = 0;
    
    // Send to server
    coop::CoopDamageHandler::Instance().SendFireEvent(fire_event);
}

void game_cl_Coop::OnCoopMessage(NET_Packet& P, u16 msg_type)
{
    switch (msg_type)
    {
    case M_COOP_PLAYER_SPAWN:
        OnRemotePlayerSpawn(P);
        break;
        
    case M_COOP_PLAYER_DESPAWN:
        OnRemotePlayerDespawn(P);
        break;
        
    case M_COOP_PLAYER_STATE:
        OnRemotePlayerState(P);
        break;
        
    case M_COOP_AI_STATE:
        OnAIState(P);
        break;
        
    case M_COOP_AI_SPAWN:
        OnAISpawn(P);
        break;
        
    case M_COOP_AI_DESPAWN:
        OnAIDespawn(P);
        break;
        
    case M_COOP_DAMAGE_EVENT:
        coop::CoopDamageHandler::Instance().OnDamageConfirm(P);
        break;
        
    default:
        // Pass to session manager
        coop::CoopSessionManager::Instance().OnMessage(P, msg_type, ClientID());
        break;
    }
}

void game_cl_Coop::OnRemotePlayerSpawn(NET_Packet& P)
{
    u32 net_id = P.r_u32();
    float pos_x = P.r_float();
    float pos_y = P.r_float();
    float pos_z = P.r_float();
    float yaw = P.r_float();
    
    // Check if this is our local player
    if (net_id == coop::CoopSessionManager::Instance().GetLocalPlayerNetID())
    {
        // Initialize local player state
        if (!m_local_player_state)
        {
            m_local_player_state = new coop::CoopPlayerState();
            m_local_player_state->SetCoopNetID(net_id);
            m_local_player_state->SetCoopLocallyOwned(true);
            
            coop::CoopEntityRegistry::Instance().RegisterEntity(m_local_player_state);
            coop::CoopPlayerRegistry::Instance().RegisterPlayer(m_local_player_state);
            coop::CoopPlayerRegistry::Instance().SetLocalPlayerNetID(net_id);
        }
        
        // Bind to local actor
        CActor* actor = smart_cast<CActor*>(Level().CurrentControlEntity());
        if (actor)
        {
            m_local_player_state->BindToActor(actor);
        }
        
        Msg("[COOP] Local player spawned at (%.1f, %.1f, %.1f)", pos_x, pos_y, pos_z);
    }
    else
    {
        // Create remote player state
        coop::CoopPlayerState* remote_player = new coop::CoopPlayerState();
        remote_player->SetCoopNetID(net_id);
        remote_player->SetCoopLocallyOwned(false);
        
        // Set initial position
        remote_player->GetSnapshotMutable().pos_x = pos_x;
        remote_player->GetSnapshotMutable().pos_y = pos_y;
        remote_player->GetSnapshotMutable().pos_z = pos_z;
        remote_player->GetSnapshotMutable().yaw = yaw;
        
        m_remote_players[net_id] = remote_player;
        coop::CoopEntityRegistry::Instance().RegisterEntity(remote_player);
        coop::CoopPlayerRegistry::Instance().RegisterPlayer(remote_player);
        
        // TODO: Spawn visual proxy actor for this player
        
        Msg("[COOP] Remote player %u spawned at (%.1f, %.1f, %.1f)", net_id, pos_x, pos_y, pos_z);
    }
}

void game_cl_Coop::OnRemotePlayerDespawn(NET_Packet& P)
{
    u32 net_id = P.r_u32();
    
    auto it = m_remote_players.find(net_id);
    if (it != m_remote_players.end())
    {
        // Clean up visual proxy
        // TODO: Destroy the actor proxy
        
        coop::CoopPlayerRegistry::Instance().UnregisterPlayer(it->second);
        coop::CoopEntityRegistry::Instance().UnregisterEntity(it->second);
        
        delete it->second;
        m_remote_players.erase(it);
        
        Msg("[COOP] Remote player %u despawned", net_id);
    }
}

void game_cl_Coop::OnRemotePlayerState(NET_Packet& P)
{
    // The snapshot starts with net_id, so we read it first and handle accordingly
    // Instead of seeking back, we pass the net_id to the snapshot read method
    
    // Read snapshot into temporary
    coop::CoopPlayerSnapshot snapshot;
    snapshot.snapshot_id = P.r_u32();
    snapshot.timestamp = P.r_u32();
    snapshot.net_id = P.r_u32();
    
    u32 net_id = snapshot.net_id;
    
    // Skip if this is our local player
    if (net_id == coop::CoopSessionManager::Instance().GetLocalPlayerNetID())
    {
        // Still need to consume the rest of the packet
        P.r_float(); P.r_float(); P.r_float();  // pos
        P.r_float(); P.r_float(); P.r_float();  // vel  
        P.r_float(); P.r_float(); P.r_float();  // rotation
        P.r_float(); P.r_float(); P.r_float();  // health, stamina, radiation
        P.r_u16(); P.r_u16(); P.r_u8(); P.r_u16();  // flags, slot, anim
        return;
    }
    
    // Continue reading the rest of the snapshot
    snapshot.pos_x = P.r_float();
    snapshot.pos_y = P.r_float();
    snapshot.pos_z = P.r_float();
    snapshot.vel_x = P.r_float();
    snapshot.vel_y = P.r_float();
    snapshot.vel_z = P.r_float();
    snapshot.yaw = P.r_float();
    snapshot.pitch = P.r_float();
    snapshot.roll = P.r_float();
    snapshot.health = P.r_float();
    snapshot.stamina = P.r_float();
    snapshot.radiation = P.r_float();
    snapshot.move_flags = P.r_u16();
    snapshot.player_flags = P.r_u16();
    snapshot.active_slot = P.r_u8();
    snapshot.anim_state = P.r_u16();
    
    // Find or create remote player
    coop::CoopPlayerState* remote_player = nullptr;
    auto it = m_remote_players.find(net_id);
    
    if (it != m_remote_players.end())
    {
        remote_player = it->second;
    }
    else
    {
        // Create new remote player
        remote_player = new coop::CoopPlayerState();
        remote_player->SetCoopNetID(net_id);
        remote_player->SetCoopLocallyOwned(false);
        
        m_remote_players[net_id] = remote_player;
        coop::CoopEntityRegistry::Instance().RegisterEntity(remote_player);
        coop::CoopPlayerRegistry::Instance().RegisterPlayer(remote_player);
    }
    
    // Apply snapshot directly
    remote_player->AddSnapshotToBuffer(snapshot);
    remote_player->GetSnapshotMutable() = snapshot;
    
    // Track for debug
    coop::CoopDebugOverlay::Instance().OnSnapshotReceived(snapshot.snapshot_id);
}

void game_cl_Coop::OnAISpawn(NET_Packet& P)
{
    // Read AI spawn data and create visual proxy
    u32 net_id = P.r_u32();
    u16 entity_type = P.r_u16();
    float pos_x = P.r_float();
    float pos_y = P.r_float();
    float pos_z = P.r_float();
    
    Msg("[COOP] AI entity %u spawned at (%.1f, %.1f, %.1f)", net_id, pos_x, pos_y, pos_z);
    
    // TODO: Create visual proxy for AI
}

void game_cl_Coop::OnAIDespawn(NET_Packet& P)
{
    u32 net_id = P.r_u32();
    
    coop::CoopAIManager::Instance().UnregisterAI(net_id);
    
    Msg("[COOP] AI entity %u despawned", net_id);
}

void game_cl_Coop::OnAIState(NET_Packet& P)
{
    coop::CoopAIManager::Instance().ApplySnapshot(P);
}

void game_cl_Coop::InterpolateRemotePlayers()
{
    u32 render_time = Device.dwTimeGlobal;
    coop::CoopPlayerRegistry::Instance().UpdateRemotePlayers(render_time);
}

void game_cl_Coop::InterpolateAI()
{
    // Handled by CoopAIManager::UpdateClientAI
}
