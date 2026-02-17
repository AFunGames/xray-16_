#pragma once
// coop_debug_overlay.h: Network debug statistics overlay
//

#include "xrCore/xrCore.h"
#include "coop_protocol.h"

class IGameFont;

namespace coop
{

/**
 * Debug overlay showing network statistics for co-op mode.
 * Displays RTT, packets/sec, entity count, snapshot info, and desync warnings.
 */
class CoopDebugOverlay
{
public:
    static CoopDebugOverlay& Instance();
    
    // Enable/disable overlay
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    void Toggle() { m_enabled = !m_enabled; }
    
    // Update statistics (called each frame)
    void Update();
    
    // Render overlay (called in HUD render)
    void Render(IGameFont& font);
    
    // Statistics input
    void OnPacketSent(u32 size);
    void OnPacketReceived(u32 size);
    void OnSnapshotReceived(SnapshotID id);
    void OnSnapshotSent(SnapshotID id);
    void OnDesyncDetected(const char* reason);
    void SetRTT(u32 rtt_ms) { m_current_rtt = rtt_ms; }
    
    // Statistics output
    u32 GetRTT() const { return m_current_rtt; }
    u32 GetPacketsSentPerSec() const { return m_packets_sent_per_sec; }
    u32 GetPacketsRecvPerSec() const { return m_packets_recv_per_sec; }
    u32 GetBytesSentPerSec() const { return m_bytes_sent_per_sec; }
    u32 GetBytesRecvPerSec() const { return m_bytes_recv_per_sec; }
    SnapshotID GetLastSnapshotID() const { return m_last_snapshot_id; }
    u32 GetDesyncCount() const { return m_desync_count; }
    
    // Reset statistics
    void Reset();
    
private:
    CoopDebugOverlay();
    ~CoopDebugOverlay();
    
    bool m_enabled;
    
    // Current stats
    u32 m_current_rtt;
    SnapshotID m_last_snapshot_id;
    SnapshotID m_last_sent_snapshot_id;
    
    // Per-second counters
    u32 m_packets_sent_per_sec;
    u32 m_packets_recv_per_sec;
    u32 m_bytes_sent_per_sec;
    u32 m_bytes_recv_per_sec;
    
    // Accumulation counters (reset each second)
    u32 m_packets_sent_acc;
    u32 m_packets_recv_acc;
    u32 m_bytes_sent_acc;
    u32 m_bytes_recv_acc;
    u32 m_last_stats_time;
    
    // Desync tracking
    u32 m_desync_count;
    u32 m_last_desync_time;
    xr_string m_last_desync_reason;
    
    // History for graphs (optional)
    static const u32 HISTORY_SIZE = 60;  // 1 second at 60fps
    u32 m_rtt_history[HISTORY_SIZE];
    u32 m_history_index;
};

// Console command to toggle overlay
void CoopDebugOverlay_Toggle();

} // namespace coop
