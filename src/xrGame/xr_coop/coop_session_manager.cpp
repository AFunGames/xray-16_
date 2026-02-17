// coop_session_manager.cpp: Implementation of co-op session management
//

#include "StdAfx.h"
#include "coop_session_manager.h"
#include "coop_net_entity.h"
#include "Level.h"
#include "xrServer.h"
#include "xrEngine/xr_level_controller.h"
#include "xrServerEntities/xrMessages.h"

namespace coop
{

//-----------------------------------------------------------------------------
// Console variables
//-----------------------------------------------------------------------------

static bool g_coop_enabled = false;
static u32 g_coop_max_players = COOP_MAX_PLAYERS;
static u16 g_coop_host_port = COOP_DEFAULT_PORT;
static u32 g_coop_interp_delay = COOP_INTERP_DELAY_MS;
static bool g_coop_debug_overlay = false;

bool IsCoopEnabled() { return g_coop_enabled; }
void SetCoopEnabled(bool enabled) { g_coop_enabled = enabled; }

//-----------------------------------------------------------------------------
// CoopSessionManager singleton
//-----------------------------------------------------------------------------

static CoopSessionManager* s_session_instance = nullptr;

CoopSessionManager& CoopSessionManager::Instance()
{
    if (!s_session_instance)
    {
        s_session_instance = new CoopSessionManager();
    }
    return *s_session_instance;
}

CoopSessionManager::CoopSessionManager()
    : m_state(COOP_SESSION_NONE)
    , m_is_host(false)
    , m_host_port(COOP_DEFAULT_PORT)
    , m_local_player_net_id(COOP_NETID_INVALID)
    , m_last_update_time(0)
{
}

CoopSessionManager::~CoopSessionManager()
{
    Shutdown();
}

void CoopSessionManager::Initialize()
{
    Msg("[COOP] Session manager initialized");
    m_state = COOP_SESSION_NONE;
    m_is_host = false;
    m_local_player_net_id = COOP_NETID_INVALID;
    ClearPlayers();
}

void CoopSessionManager::Shutdown()
{
    LeaveSession();
    ClearPlayers();
    Msg("[COOP] Session manager shutdown");
}

//-----------------------------------------------------------------------------
// Session lifecycle
//-----------------------------------------------------------------------------

bool CoopSessionManager::HostSession(const char* level_name, u8 max_players, const char* password)
{
    if (m_state != COOP_SESSION_NONE)
    {
        Msg("![COOP] Cannot host: already in session");
        return false;
    }
    
    if (!level_name || !level_name[0])
    {
        Msg("![COOP] Cannot host: invalid level name");
        return false;
    }
    
    Msg("[COOP] Hosting session on level '%s', max players: %d", level_name, max_players);
    
    // Set up session info
    m_session_info.protocol_version = COOP_PROTOCOL_VERSION;
    m_session_info.max_players = max_players > COOP_MAX_PLAYERS ? COOP_MAX_PLAYERS : max_players;
    m_session_info.current_players = 1;
    m_session_info.password_required = (password && password[0]) ? 1 : 0;
    xr_strcpy(m_session_info.level_name, level_name);
    xr_strcpy(m_session_info.host_name, "Host");
    
    m_current_level = level_name;
    m_password = password ? password : "";
    m_is_host = true;
    m_state = COOP_SESSION_HOSTING;
    
    // Allocate host player ID
    m_local_player_net_id = CoopEntityRegistry::Instance().AllocateNetID(true);
    
    // Create host player entry
    CoopPlayer host_player;
    host_player.net_id = m_local_player_net_id;
    host_player.name = "Host";
    host_player.flags = COOP_PLAYER_FLAG_HOST | COOP_PLAYER_FLAG_READY;
    host_player.join_time = Device.dwTimeGlobal;
    m_players.push_back(host_player);
    
    Msg("[COOP] Host session created, local player ID: %u", m_local_player_net_id);
    return true;
}

bool CoopSessionManager::JoinSession(const char* host_address, u16 port, const char* password)
{
    if (m_state != COOP_SESSION_NONE)
    {
        Msg("![COOP] Cannot join: already in session");
        return false;
    }
    
    if (!host_address || !host_address[0])
    {
        Msg("![COOP] Cannot join: invalid host address");
        return false;
    }
    
    Msg("[COOP] Joining session at %s:%u", host_address, port);
    
    m_host_address = host_address;
    m_host_port = port;
    m_password = password ? password : "";
    m_is_host = false;
    m_state = COOP_SESSION_CONNECTING;
    
    // Connection will be completed when we receive session info from host
    return true;
}

void CoopSessionManager::LeaveSession()
{
    if (m_state == COOP_SESSION_NONE)
        return;
    
    Msg("[COOP] Leaving session");
    
    // Notify other players if we're in an active session
    if (m_state == COOP_SESSION_ACTIVE)
    {
        // TODO: Send leave notification
    }
    
    // Clean up
    m_state = COOP_SESSION_NONE;
    m_is_host = false;
    m_local_player_net_id = COOP_NETID_INVALID;
    ClearPlayers();
    
    // Clear entity registry
    CoopEntityRegistry::Instance().Clear();
}

//-----------------------------------------------------------------------------
// Player management
//-----------------------------------------------------------------------------

const CoopPlayer* CoopSessionManager::GetLocalPlayer() const
{
    return GetPlayer(m_local_player_net_id);
}

const CoopPlayer* CoopSessionManager::GetPlayer(u32 net_id) const
{
    for (const auto& player : m_players)
    {
        if (player.net_id == net_id)
            return &player;
    }
    return nullptr;
}

CoopPlayer* CoopSessionManager::FindPlayer(u32 net_id)
{
    for (auto& player : m_players)
    {
        if (player.net_id == net_id)
            return &player;
    }
    return nullptr;
}

void CoopSessionManager::RemovePlayer(u32 net_id)
{
    for (auto it = m_players.begin(); it != m_players.end(); ++it)
    {
        if (it->net_id == net_id)
        {
            m_players.erase(it);
            break;
        }
    }
}

void CoopSessionManager::ClearPlayers()
{
    m_players.clear();
}

void CoopSessionManager::OnPlayerJoined(const CoopPlayer& player)
{
    Msg("[COOP] Player '%s' joined (ID: %u)", player.name.c_str(), player.net_id);
    m_players.push_back(player);
    m_session_info.current_players = static_cast<u8>(m_players.size());
}

void CoopSessionManager::OnPlayerLeft(u32 net_id)
{
    const CoopPlayer* player = GetPlayer(net_id);
    if (player)
    {
        Msg("[COOP] Player '%s' left (ID: %u)", player->name.c_str(), net_id);
    }
    
    RemovePlayer(net_id);
    CoopEntityRegistry::Instance().UnregisterEntity(net_id);
    m_session_info.current_players = static_cast<u8>(m_players.size());
}

void CoopSessionManager::OnPlayerReady(u32 net_id)
{
    CoopPlayer* player = FindPlayer(net_id);
    if (player)
    {
        player->flags |= COOP_PLAYER_FLAG_READY;
        Msg("[COOP] Player '%s' is ready", player->name.c_str());
    }
}

//-----------------------------------------------------------------------------
// Level events
//-----------------------------------------------------------------------------

void CoopSessionManager::OnLevelLoaded()
{
    if (m_state == COOP_SESSION_LOADING)
    {
        m_state = COOP_SESSION_ACTIVE;
        Msg("[COOP] Level loaded, session now active");
    }
}

void CoopSessionManager::OnLevelUnloaded()
{
    if (m_state == COOP_SESSION_ACTIVE)
    {
        m_state = COOP_SESSION_HOSTING;  // Back to hosting state if host
        CoopEntityRegistry::Instance().Clear();
        Msg("[COOP] Level unloaded");
    }
}

//-----------------------------------------------------------------------------
// Network message handling
//-----------------------------------------------------------------------------

void CoopSessionManager::OnMessage(NET_Packet& P, u16 msg_type, ClientID sender)
{
    switch (msg_type)
    {
    case M_COOP_SESSION_INFO:
        HandleSessionInfo(P, sender);
        break;
    case M_COOP_JOIN_REQUEST:
        HandleJoinRequest(P, sender);
        break;
    case M_COOP_JOIN_RESPONSE:
        HandleJoinResponse(P, sender);
        break;
    case M_COOP_PLAYER_JOINED:
        HandlePlayerJoined(P, sender);
        break;
    case M_COOP_PLAYER_LEFT:
        HandlePlayerLeft(P, sender);
        break;
    case M_COOP_CHAT_MESSAGE:
        HandleChatMessage(P, sender);
        break;
    case M_COOP_PING_MARKER:
        HandlePingMarker(P, sender);
        break;
    default:
        break;
    }
}

void CoopSessionManager::HandleSessionInfo(NET_Packet& P, ClientID sender)
{
    if (!IsClient())
        return;
    
    // Read session info from host
    P.r(&m_session_info, sizeof(CoopSessionInfo));
    
    // Verify protocol version
    if (m_session_info.protocol_version != COOP_PROTOCOL_VERSION)
    {
        Msg("![COOP] Protocol version mismatch: host=%u, client=%u",
            m_session_info.protocol_version, COOP_PROTOCOL_VERSION);
        LeaveSession();
        return;
    }
    
    m_current_level = m_session_info.level_name;
    Msg("[COOP] Received session info: level='%s', players=%u/%u",
        m_session_info.level_name, m_session_info.current_players, m_session_info.max_players);
}

void CoopSessionManager::HandleJoinRequest(NET_Packet& P, ClientID sender)
{
    if (!IsHost())
        return;
    
    // Read join request
    u32 client_protocol_version = P.r_u32();
    char client_name[32];
    P.r_stringZ(client_name, sizeof(client_name));
    
    // TODO: Read and verify password if required
    
    // Prepare response
    NET_Packet response;
    response.w_begin(M_COOP_JOIN_RESPONSE);
    
    // Check protocol version
    if (client_protocol_version != COOP_PROTOCOL_VERSION)
    {
        response.w_u8(JOIN_DENIED_VERSION);
        SendToPlayer(0, response, true);  // Send denial to client
        Msg("![COOP] Join denied: protocol version mismatch (client=%u, server=%u)",
            client_protocol_version, COOP_PROTOCOL_VERSION);
        return;
    }
    
    // Check if session is full
    if (m_players.size() >= m_session_info.max_players)
    {
        response.w_u8(JOIN_DENIED_FULL);
        SendToPlayer(0, response, true);  // Send denial to client
        Msg("![COOP] Join denied: session full");
        return;
    }
    
    // Accept the player
    u32 new_player_id = CoopEntityRegistry::Instance().AllocateNetID(true);
    
    response.w_u8(JOIN_OK);
    response.w_u32(new_player_id);
    response.w(&m_session_info, sizeof(CoopSessionInfo));
    
    // Send acceptance response to client
    SendToPlayer(new_player_id, response, true);
    
    // Create player entry
    CoopPlayer new_player;
    new_player.net_id = new_player_id;
    new_player.client_id = sender;
    new_player.name = client_name;
    new_player.flags = 0;
    new_player.join_time = Device.dwTimeGlobal;
    
    OnPlayerJoined(new_player);
    
    // Send join notification to all other players
    BroadcastPlayerList();
    
    Msg("[COOP] Player '%s' joined with ID %u", client_name, new_player_id);
}

void CoopSessionManager::HandleJoinResponse(NET_Packet& P, ClientID sender)
{
    if (!IsClient() || m_state != COOP_SESSION_CONNECTING)
        return;
    
    EJoinResponseCode response_code = static_cast<EJoinResponseCode>(P.r_u8());
    
    if (response_code != JOIN_OK)
    {
        Msg("![COOP] Join denied: code=%d", response_code);
        LeaveSession();
        return;
    }
    
    // Read assigned player ID
    m_local_player_net_id = P.r_u32();
    P.r(&m_session_info, sizeof(CoopSessionInfo));
    
    m_current_level = m_session_info.level_name;
    m_state = COOP_SESSION_CONNECTED;
    
    Msg("[COOP] Join accepted, local player ID: %u", m_local_player_net_id);
}

void CoopSessionManager::HandlePlayerJoined(NET_Packet& P, ClientID sender)
{
    CoopPlayer player;
    player.net_id = P.r_u32();
    
    char name[32];
    P.r_stringZ(name, sizeof(name));
    player.name = name;
    
    player.flags = P.r_u16();
    player.join_time = Device.dwTimeGlobal;
    
    OnPlayerJoined(player);
}

void CoopSessionManager::HandlePlayerLeft(NET_Packet& P, ClientID sender)
{
    u32 player_net_id = P.r_u32();
    OnPlayerLeft(player_net_id);
}

void CoopSessionManager::HandleChatMessage(NET_Packet& P, ClientID sender)
{
    CoopChatMessage msg;
    msg.timestamp = P.r_u32();
    msg.sender_net_id = P.r_u32();
    msg.message_length = P.r_u8();
    
    char message_text[256];
    if (msg.message_length > 0 && msg.message_length < sizeof(message_text))
    {
        P.r(message_text, msg.message_length);
        message_text[msg.message_length] = '\0';
        
        const CoopPlayer* player = GetPlayer(msg.sender_net_id);
        const char* player_name = player ? player->name.c_str() : "Unknown";
        
        Msg("[COOP Chat] %s: %s", player_name, message_text);
        
        // TODO: Display in game UI
    }
}

void CoopSessionManager::HandlePingMarker(NET_Packet& P, ClientID sender)
{
    CoopPingMarker marker;
    marker.timestamp = P.r_u32();
    marker.placer_net_id = P.r_u32();
    marker.pos_x = P.r_float();
    marker.pos_y = P.r_float();
    marker.pos_z = P.r_float();
    marker.marker_type = P.r_u8();
    
    const CoopPlayer* player = GetPlayer(marker.placer_net_id);
    const char* player_name = player ? player->name.c_str() : "Unknown";
    
    Msg("[COOP] Ping marker from %s at (%.1f, %.1f, %.1f)",
        player_name, marker.pos_x, marker.pos_y, marker.pos_z);
    
    // TODO: Display marker in game world
}

//-----------------------------------------------------------------------------
// Send messages
//-----------------------------------------------------------------------------

void CoopSessionManager::SendToAll(NET_Packet& P, bool reliable)
{
    // TODO: Implement via Level or Server
}

void CoopSessionManager::SendToHost(NET_Packet& P, bool reliable)
{
    // TODO: Implement via Level
}

void CoopSessionManager::SendToPlayer(u32 net_id, NET_Packet& P, bool reliable)
{
    // TODO: Implement via Server
}

void CoopSessionManager::SendChatMessage(const char* message)
{
    if (!message || !message[0] || !IsInSession())
        return;
    
    size_t len = xr_strlen(message);
    if (len > 255)
        len = 255;
    
    NET_Packet P;
    P.w_begin(M_COOP_CHAT_MESSAGE);
    P.w_u32(Device.dwTimeGlobal);
    P.w_u32(m_local_player_net_id);
    P.w_u8(static_cast<u8>(len));
    P.w(message, static_cast<u32>(len));
    
    SendToAll(P, true);
}

void CoopSessionManager::PlacePingMarker(const Fvector& position, u8 marker_type)
{
    if (!IsInSession())
        return;
    
    NET_Packet P;
    P.w_begin(M_COOP_PING_MARKER);
    P.w_u32(Device.dwTimeGlobal);
    P.w_u32(m_local_player_net_id);
    P.w_float(position.x);
    P.w_float(position.y);
    P.w_float(position.z);
    P.w_u8(marker_type);
    
    SendToAll(P, true);
}

void CoopSessionManager::BroadcastPlayerList()
{
    // TODO: Send full player list to all clients
}

//-----------------------------------------------------------------------------
// Update
//-----------------------------------------------------------------------------

void CoopSessionManager::Update()
{
    if (m_state == COOP_SESSION_NONE)
        return;
    
    u32 current_time = Device.dwTimeGlobal;
    
    // Update at fixed rate
    if (current_time - m_last_update_time < 50)  // 20 Hz
        return;
    
    m_last_update_time = current_time;
    
    // TODO: Process pending messages, update player states, etc.
}

} // namespace coop
