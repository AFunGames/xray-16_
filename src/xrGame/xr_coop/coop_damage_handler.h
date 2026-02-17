#pragma once
// coop_damage_handler.h: Server-side damage validation and replication
//

#include "xrCore/xrCore.h"
#include "coop_protocol.h"

class NET_Packet;
class CGameObject;

namespace coop
{

/**
 * Handles damage validation and replication for co-op mode.
 * Server validates all damage and broadcasts results.
 */
class CoopDamageHandler
{
public:
    static CoopDamageHandler& Instance();
    
    // Server-side: Process fire event from client
    void OnFireEvent(const CoopFireEvent& fire_event, ClientID sender);
    
    // Server-side: Process damage request
    void OnDamageRequest(const CoopDamageEvent& damage_event, ClientID sender);
    
    // Server-side: Apply validated damage
    void ApplyDamage(u32 target_net_id, u32 source_net_id, float damage, 
                     const Fvector& hit_pos, const Fvector& hit_dir, u16 hit_type, u16 bone_id);
    
    // Client-side: Receive damage confirmation from server
    void OnDamageConfirm(NET_Packet& P);
    
    // Client-side: Send fire event to server
    void SendFireEvent(const CoopFireEvent& fire_event);
    
    // Broadcast damage event to all clients
    void BroadcastDamageEvent(const CoopDamageEvent& damage_event);
    
    // Validation
    bool ValidateFireEvent(const CoopFireEvent& fire_event, ClientID sender);
    bool ValidateDamageEvent(const CoopDamageEvent& damage_event, ClientID sender);
    
private:
    CoopDamageHandler();
    ~CoopDamageHandler();
    
    // Rate limiting to prevent spam
    struct FireRateInfo
    {
        u32 last_fire_time;
        u32 fire_count;
    };
    xr_map<u32, FireRateInfo> m_fire_rates;
    
    // Check fire rate limiting
    bool CheckFireRate(u32 shooter_net_id, u32 current_time);
};

} // namespace coop
