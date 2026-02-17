#pragma once
// coop_session_manager.h: Manages co-op session lifecycle
//

#include "xrCore/xrCore.h"
#include "coop_protocol.h"
#include "xrCommon/xr_string.h"
#include "xrCommon/xr_vector.h"

class NET_Packet;
class xrServer;
class CLevel;

namespace coop
{

// Forward declarations
class ICoopNetEntity;

// Player info for session management
struct CoopPlayer
{
    u32 net_id;
    ClientID client_id;
    xr_string name;
    u16 flags;
    u32 ping;
    u32 join_time;
    
    CoopPlayer()
        : net_id(COOP_NETID_INVALID), flags(0), ping(0), join_time(0) {}
};

// Session state enumeration
enum ECoopSessionState
{
    COOP_SESSION_NONE = 0,      // No session
    COOP_SESSION_HOSTING,       // Hosting a session
    COOP_SESSION_CONNECTING,    // Connecting to a host
    COOP_SESSION_CONNECTED,     // Connected to a session
    COOP_SESSION_LOADING,       // Loading level
    COOP_SESSION_ACTIVE,        // Session active, playing
};

/**
 * Manages co-op session lifecycle including hosting, joining, and player management.
 */
class CoopSessionManager
{
public:
    static CoopSessionManager& Instance();
    
    // Initialization
    void Initialize();
    void Shutdown();
    
    // Session lifecycle
    bool HostSession(const char* level_name, u8 max_players = COOP_MAX_PLAYERS, const char* password = nullptr);
    bool JoinSession(const char* host_address, u16 port = COOP_DEFAULT_PORT, const char* password = nullptr);
    void LeaveSession();
    
    // State queries
    ECoopSessionState GetState() const { return m_state; }
    bool IsInSession() const { return m_state >= COOP_SESSION_CONNECTING; }
    bool IsHost() const { return m_state == COOP_SESSION_HOSTING || (m_state == COOP_SESSION_ACTIVE && m_is_host); }
    bool IsClient() const { return IsInSession() && !IsHost(); }
    bool IsActive() const { return m_state == COOP_SESSION_ACTIVE; }
    
    // Session info
    const CoopSessionInfo& GetSessionInfo() const { return m_session_info; }
    const xr_string& GetCurrentLevel() const { return m_current_level; }
    
    // Player management
    u32 GetLocalPlayerNetID() const { return m_local_player_net_id; }
    const CoopPlayer* GetLocalPlayer() const;
    const CoopPlayer* GetPlayer(u32 net_id) const;
    const xr_vector<CoopPlayer>& GetPlayers() const { return m_players; }
    u32 GetPlayerCount() const { return static_cast<u32>(m_players.size()); }
    
    // Network message handling (called from Level/Server)
    void OnMessage(NET_Packet& P, u16 msg_type, ClientID sender);
    
    // Player events
    void OnPlayerJoined(const CoopPlayer& player);
    void OnPlayerLeft(u32 net_id);
    void OnPlayerReady(u32 net_id);
    
    // Level events
    void OnLevelLoaded();
    void OnLevelUnloaded();
    
    // Update (called each frame)
    void Update();
    
    // Send messages
    void SendToAll(NET_Packet& P, bool reliable = true);
    void SendToHost(NET_Packet& P, bool reliable = true);
    void SendToPlayer(u32 net_id, NET_Packet& P, bool reliable = true);
    
    // Chat
    void SendChatMessage(const char* message);
    
    // Ping/marker system
    void PlacePingMarker(const Fvector& position, u8 marker_type = 0);
    
private:
    CoopSessionManager();
    ~CoopSessionManager();
    
    CoopSessionManager(const CoopSessionManager&) = delete;
    CoopSessionManager& operator=(const CoopSessionManager&) = delete;
    
    // Internal state
    ECoopSessionState m_state;
    bool m_is_host;
    CoopSessionInfo m_session_info;
    xr_string m_current_level;
    xr_string m_host_address;
    u16 m_host_port;
    xr_string m_password;
    
    // Players
    xr_vector<CoopPlayer> m_players;
    u32 m_local_player_net_id;
    
    // Timing
    u32 m_last_update_time;
    
    // Message handlers
    void HandleSessionInfo(NET_Packet& P, ClientID sender);
    void HandleJoinRequest(NET_Packet& P, ClientID sender);
    void HandleJoinResponse(NET_Packet& P, ClientID sender);
    void HandlePlayerJoined(NET_Packet& P, ClientID sender);
    void HandlePlayerLeft(NET_Packet& P, ClientID sender);
    void HandleChatMessage(NET_Packet& P, ClientID sender);
    void HandlePingMarker(NET_Packet& P, ClientID sender);
    
    // Internal helpers
    CoopPlayer* FindPlayer(u32 net_id);
    void RemovePlayer(u32 net_id);
    void ClearPlayers();
    void BroadcastPlayerList();
};

// Console variable accessors (defined in coop_console.cpp)
bool IsCo_opEnabled();
void SetCoopEnabled(bool enabled);

} // namespace coop
