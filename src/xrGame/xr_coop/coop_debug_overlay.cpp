// coop_debug_overlay.cpp: Implementation of network debug overlay
//

#include "StdAfx.h"
#include "coop_debug_overlay.h"
#include "coop_session_manager.h"
#include "coop_net_entity.h"
#include "coop_player_state.h"
#include "coop_ai_sync.h"
#include "xrEngine/GameFont.h"

namespace coop
{

static CoopDebugOverlay* s_overlay_instance = nullptr;

CoopDebugOverlay& CoopDebugOverlay::Instance()
{
    if (!s_overlay_instance)
    {
        s_overlay_instance = new CoopDebugOverlay();
    }
    return *s_overlay_instance;
}

CoopDebugOverlay::CoopDebugOverlay()
    : m_enabled(false)
    , m_current_rtt(0)
    , m_last_snapshot_id(0)
    , m_last_sent_snapshot_id(0)
    , m_packets_sent_per_sec(0)
    , m_packets_recv_per_sec(0)
    , m_bytes_sent_per_sec(0)
    , m_bytes_recv_per_sec(0)
    , m_packets_sent_acc(0)
    , m_packets_recv_acc(0)
    , m_bytes_sent_acc(0)
    , m_bytes_recv_acc(0)
    , m_last_stats_time(0)
    , m_desync_count(0)
    , m_last_desync_time(0)
    , m_history_index(0)
{
    ZeroMemory(m_rtt_history, sizeof(m_rtt_history));
}

CoopDebugOverlay::~CoopDebugOverlay()
{
}

void CoopDebugOverlay::Update()
{
    if (!m_enabled)
        return;
    
    u32 current_time = Device.dwTimeGlobal;
    
    // Update per-second stats
    if (current_time - m_last_stats_time >= 1000)
    {
        m_packets_sent_per_sec = m_packets_sent_acc;
        m_packets_recv_per_sec = m_packets_recv_acc;
        m_bytes_sent_per_sec = m_bytes_sent_acc;
        m_bytes_recv_per_sec = m_bytes_recv_acc;
        
        m_packets_sent_acc = 0;
        m_packets_recv_acc = 0;
        m_bytes_sent_acc = 0;
        m_bytes_recv_acc = 0;
        
        m_last_stats_time = current_time;
    }
    
    // Update RTT history
    m_rtt_history[m_history_index] = m_current_rtt;
    m_history_index = (m_history_index + 1) % HISTORY_SIZE;
}

void CoopDebugOverlay::Render(IGameFont& font)
{
    if (!m_enabled)
        return;
    
    const CoopSessionManager& session = CoopSessionManager::Instance();
    if (!session.IsInSession())
        return;
    
    // Starting position
    float x = 10.0f;
    float y = 50.0f;
    float line_height = 14.0f;
    
    // Header
    font.SetColor(color_rgba(0, 255, 0, 255));
    font.OutI(x, y, "=== COOP DEBUG ===");
    y += line_height;
    
    // Session info
    font.SetColor(color_rgba(255, 255, 255, 255));
    
    const char* state_str = "UNKNOWN";
    switch (session.GetState())
    {
    case COOP_SESSION_NONE:       state_str = "NONE"; break;
    case COOP_SESSION_HOSTING:    state_str = "HOSTING"; break;
    case COOP_SESSION_CONNECTING: state_str = "CONNECTING"; break;
    case COOP_SESSION_CONNECTED:  state_str = "CONNECTED"; break;
    case COOP_SESSION_LOADING:    state_str = "LOADING"; break;
    case COOP_SESSION_ACTIVE:     state_str = "ACTIVE"; break;
    }
    
    font.OutI(x, y, "Session: %s", state_str);
    y += line_height;
    
    font.OutI(x, y, "Players: %u/%u", 
              session.GetPlayerCount(), 
              session.GetSessionInfo().max_players);
    y += line_height;
    
    font.OutI(x, y, "Local ID: %u", session.GetLocalPlayerNetID());
    y += line_height;
    
    // Network stats
    y += line_height / 2;
    font.SetColor(color_rgba(0, 255, 255, 255));
    font.OutI(x, y, "--- Network ---");
    y += line_height;
    font.SetColor(color_rgba(255, 255, 255, 255));
    
    // RTT with color based on quality
    u32 rtt_color = color_rgba(0, 255, 0, 255);  // Green
    if (m_current_rtt > 100)
        rtt_color = color_rgba(255, 255, 0, 255);  // Yellow
    if (m_current_rtt > 200)
        rtt_color = color_rgba(255, 128, 0, 255);  // Orange
    if (m_current_rtt > 300)
        rtt_color = color_rgba(255, 0, 0, 255);    // Red
    
    font.SetColor(rtt_color);
    font.OutI(x, y, "RTT: %u ms", m_current_rtt);
    y += line_height;
    font.SetColor(color_rgba(255, 255, 255, 255));
    
    font.OutI(x, y, "Packets/s: %u sent, %u recv", 
              m_packets_sent_per_sec, m_packets_recv_per_sec);
    y += line_height;
    
    font.OutI(x, y, "Bytes/s: %u sent, %u recv", 
              m_bytes_sent_per_sec, m_bytes_recv_per_sec);
    y += line_height;
    
    // Snapshot info
    y += line_height / 2;
    font.SetColor(color_rgba(0, 255, 255, 255));
    font.OutI(x, y, "--- Snapshots ---");
    y += line_height;
    font.SetColor(color_rgba(255, 255, 255, 255));
    
    font.OutI(x, y, "Last recv: %u", m_last_snapshot_id);
    y += line_height;
    
    font.OutI(x, y, "Last sent: %u", m_last_sent_snapshot_id);
    y += line_height;
    
    // Entity counts
    y += line_height / 2;
    font.SetColor(color_rgba(0, 255, 255, 255));
    font.OutI(x, y, "--- Entities ---");
    y += line_height;
    font.SetColor(color_rgba(255, 255, 255, 255));
    
    font.OutI(x, y, "Total: %u", CoopEntityRegistry::Instance().GetEntityCount());
    y += line_height;
    
    font.OutI(x, y, "Players: %u", CoopPlayerRegistry::Instance().GetAllPlayers().size());
    y += line_height;
    
    font.OutI(x, y, "AI: %u", CoopAIManager::Instance().GetAICount());
    y += line_height;
    
    // Desync warnings
    if (m_desync_count > 0)
    {
        y += line_height / 2;
        font.SetColor(color_rgba(255, 0, 0, 255));
        font.OutI(x, y, "!!! DESYNC WARNINGS: %u !!!", m_desync_count);
        y += line_height;
        
        if (!m_last_desync_reason.empty())
        {
            font.OutI(x, y, "Last: %s", m_last_desync_reason.c_str());
            y += line_height;
        }
    }
    
    // Player list
    y += line_height / 2;
    font.SetColor(color_rgba(0, 255, 255, 255));
    font.OutI(x, y, "--- Players ---");
    y += line_height;
    
    const auto& players = session.GetPlayers();
    for (const auto& player : players)
    {
        bool is_local = (player.net_id == session.GetLocalPlayerNetID());
        bool is_host = (player.flags & COOP_PLAYER_FLAG_HOST) != 0;
        
        font.SetColor(is_local ? color_rgba(0, 255, 0, 255) : color_rgba(255, 255, 255, 255));
        
        font.OutI(x, y, "[%u] %s %s%s ping:%u", 
                  player.net_id,
                  player.name.c_str(),
                  is_host ? "(HOST)" : "",
                  is_local ? "(YOU)" : "",
                  player.ping);
        y += line_height;
    }
}

void CoopDebugOverlay::OnPacketSent(u32 size)
{
    m_packets_sent_acc++;
    m_bytes_sent_acc += size;
}

void CoopDebugOverlay::OnPacketReceived(u32 size)
{
    m_packets_recv_acc++;
    m_bytes_recv_acc += size;
}

void CoopDebugOverlay::OnSnapshotReceived(SnapshotID id)
{
    // Check for out-of-order snapshots, accounting for wraparound
    // A snapshot is considered out-of-order if:
    // - It's less than the last received, AND
    // - The difference is small (not a wraparound case)
    // For wraparound: when last_id is near max and new id is near 0, 
    // the difference will be very large, which is fine
    const SnapshotID max_reasonable_gap = 1000;
    
    if (m_last_snapshot_id != 0 && id != 0)
    {
        // Check if this is a genuine out-of-order (not wraparound)
        if (id < m_last_snapshot_id)
        {
            SnapshotID diff = m_last_snapshot_id - id;
            // Only flag as desync if the gap is small (genuine out-of-order)
            // Large gaps indicate wraparound which is expected
            if (diff < max_reasonable_gap)
            {
                OnDesyncDetected("Out-of-order snapshot");
            }
        }
    }
    
    m_last_snapshot_id = id;
}

void CoopDebugOverlay::OnSnapshotSent(SnapshotID id)
{
    m_last_sent_snapshot_id = id;
}

void CoopDebugOverlay::OnDesyncDetected(const char* reason)
{
    m_desync_count++;
    m_last_desync_time = Device.dwTimeGlobal;
    m_last_desync_reason = reason ? reason : "Unknown";
    
    Msg("![COOP DESYNC] %s (total: %u)", reason, m_desync_count);
}

void CoopDebugOverlay::Reset()
{
    m_current_rtt = 0;
    m_last_snapshot_id = 0;
    m_last_sent_snapshot_id = 0;
    m_packets_sent_per_sec = 0;
    m_packets_recv_per_sec = 0;
    m_bytes_sent_per_sec = 0;
    m_bytes_recv_per_sec = 0;
    m_packets_sent_acc = 0;
    m_packets_recv_acc = 0;
    m_bytes_sent_acc = 0;
    m_bytes_recv_acc = 0;
    m_desync_count = 0;
    m_last_desync_reason.clear();
    ZeroMemory(m_rtt_history, sizeof(m_rtt_history));
    m_history_index = 0;
}

void CoopDebugOverlay_Toggle()
{
    CoopDebugOverlay::Instance().Toggle();
    bool enabled = CoopDebugOverlay::Instance().IsEnabled();
    Msg("[COOP] Debug overlay %s", enabled ? "enabled" : "disabled");
}

} // namespace coop
