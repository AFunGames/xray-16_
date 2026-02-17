// coop_damage_handler.cpp: Implementation of server-side damage validation
//

#include "StdAfx.h"
#include "coop_damage_handler.h"
#include "coop_session_manager.h"
#include "coop_net_entity.h"
#include "Level.h"
#include "xrServer.h"
#include "Entity.h"
#include "Hit.h"
#include "xrCDB/Intersect.hpp"

namespace coop
{

static CoopDamageHandler* s_damage_handler_instance = nullptr;

CoopDamageHandler& CoopDamageHandler::Instance()
{
    if (!s_damage_handler_instance)
    {
        s_damage_handler_instance = new CoopDamageHandler();
    }
    return *s_damage_handler_instance;
}

CoopDamageHandler::CoopDamageHandler()
{
}

CoopDamageHandler::~CoopDamageHandler()
{
    m_fire_rates.clear();
}

void CoopDamageHandler::OnFireEvent(const CoopFireEvent& fire_event, ClientID sender)
{
    if (!CoopSessionManager::Instance().IsHost())
        return;
    
    // Validate the fire event
    if (!ValidateFireEvent(fire_event, sender))
    {
        Msg("![COOP] Invalid fire event from client, ignoring");
        return;
    }
    
    // Perform server-side raycast to determine what was hit
    Fvector origin, direction;
    origin.set(fire_event.origin_x, fire_event.origin_y, fire_event.origin_z);
    direction.set(fire_event.dir_x, fire_event.dir_y, fire_event.dir_z);
    direction.normalize();
    
    // Maximum fire range
    const float max_range = 500.0f;
    
    // Do raycast
    collide::rq_result result;
    collide::ray_defs ray_query(origin, direction, max_range, CDB::OPT_ONLYNEAREST, collide::rqtBoth);
    
    // Get the level for collision testing
    CLevel& level = Level();
    
    // For MVP, we use a simplified hit detection
    // In production, this would use the proper collision system
    
    Fvector hit_pos = origin;
    hit_pos.mad(direction, max_range);
    
    // Find potential targets along the ray
    // This is a simplified implementation - real implementation would use proper raycasting
    const float hit_radius = 0.5f;  // Simplified hit detection radius
    
    // For now, broadcast the fire event to all clients for visual effects
    NET_Packet P;
    P.w_begin(M_COOP_FIRE_EVENT);
    P.w_u32(fire_event.timestamp);
    P.w_u32(fire_event.shooter_net_id);
    P.w_u16(fire_event.weapon_id);
    P.w_float(fire_event.origin_x);
    P.w_float(fire_event.origin_y);
    P.w_float(fire_event.origin_z);
    P.w_float(fire_event.dir_x);
    P.w_float(fire_event.dir_y);
    P.w_float(fire_event.dir_z);
    P.w_u8(fire_event.fire_mode);
    P.w_u8(fire_event.ammo_type);
    
    CoopSessionManager::Instance().SendToAll(P, false);
    
    Msg("[COOP] Fire event processed from player %u", fire_event.shooter_net_id);
}

void CoopDamageHandler::OnDamageRequest(const CoopDamageEvent& damage_event, ClientID sender)
{
    if (!CoopSessionManager::Instance().IsHost())
        return;
    
    // Validate the damage request
    if (!ValidateDamageEvent(damage_event, sender))
    {
        Msg("![COOP] Invalid damage event from client, ignoring");
        return;
    }
    
    // Apply the damage on the server
    ApplyDamage(damage_event.target_net_id, damage_event.source_net_id,
                damage_event.damage, 
                Fvector().set(damage_event.hit_x, damage_event.hit_y, damage_event.hit_z),
                Fvector().set(damage_event.dir_x, damage_event.dir_y, damage_event.dir_z),
                damage_event.hit_type, damage_event.bone_id);
}

void CoopDamageHandler::ApplyDamage(u32 target_net_id, u32 source_net_id, float damage,
                                     const Fvector& hit_pos, const Fvector& hit_dir, 
                                     u16 hit_type, u16 bone_id)
{
    if (!CoopSessionManager::Instance().IsHost())
        return;
    
    // Get the target entity from the registry
    ICoopNetEntity* target_entity = CoopEntityRegistry::Instance().GetEntity(target_net_id);
    if (!target_entity)
    {
        Msg("![COOP] Damage target not found: net_id=%u", target_net_id);
        return;
    }
    
    // Create damage event for replication
    CoopDamageEvent damage_event;
    damage_event.timestamp = Device.dwTimeGlobal;
    damage_event.target_net_id = target_net_id;
    damage_event.source_net_id = source_net_id;
    damage_event.damage = damage;
    damage_event.impulse = damage * 10.0f;  // Simplified impulse calculation
    damage_event.hit_x = hit_pos.x;
    damage_event.hit_y = hit_pos.y;
    damage_event.hit_z = hit_pos.z;
    damage_event.dir_x = hit_dir.x;
    damage_event.dir_y = hit_dir.y;
    damage_event.dir_z = hit_dir.z;
    damage_event.hit_type = hit_type;
    damage_event.bone_id = bone_id;
    
    // Apply damage locally on server
    // The actual damage application would go through the game's hit system
    // For now, we just mark the entity as dirty to trigger state sync
    target_entity->MarkCoopDirty();
    
    // Broadcast damage event to all clients
    BroadcastDamageEvent(damage_event);
    
    Msg("[COOP] Damage applied: target=%u, source=%u, damage=%.1f", 
        target_net_id, source_net_id, damage);
}

void CoopDamageHandler::OnDamageConfirm(NET_Packet& P)
{
    // Read damage event from server
    CoopDamageEvent damage_event;
    damage_event.timestamp = P.r_u32();
    damage_event.target_net_id = P.r_u32();
    damage_event.source_net_id = P.r_u32();
    damage_event.damage = P.r_float();
    damage_event.impulse = P.r_float();
    damage_event.hit_x = P.r_float();
    damage_event.hit_y = P.r_float();
    damage_event.hit_z = P.r_float();
    damage_event.dir_x = P.r_float();
    damage_event.dir_y = P.r_float();
    damage_event.dir_z = P.r_float();
    damage_event.hit_type = P.r_u16();
    damage_event.bone_id = P.r_u16();
    
    // Apply visual effects on client
    // Get target entity and apply hit effect
    ICoopNetEntity* target_entity = CoopEntityRegistry::Instance().GetEntity(damage_event.target_net_id);
    if (target_entity)
    {
        // Mark entity to update state on next frame
        target_entity->MarkCoopDirty();
    }
    
    // TODO: Play hit marker, blood effects, etc.
}

void CoopDamageHandler::SendFireEvent(const CoopFireEvent& fire_event)
{
    if (!CoopSessionManager::Instance().IsInSession())
        return;
    
    NET_Packet P;
    P.w_begin(M_COOP_FIRE_EVENT);
    P.w_u32(fire_event.timestamp);
    P.w_u32(fire_event.shooter_net_id);
    P.w_u16(fire_event.weapon_id);
    P.w_float(fire_event.origin_x);
    P.w_float(fire_event.origin_y);
    P.w_float(fire_event.origin_z);
    P.w_float(fire_event.dir_x);
    P.w_float(fire_event.dir_y);
    P.w_float(fire_event.dir_z);
    P.w_u8(fire_event.fire_mode);
    P.w_u8(fire_event.ammo_type);
    
    CoopSessionManager::Instance().SendToHost(P, true);
}

void CoopDamageHandler::BroadcastDamageEvent(const CoopDamageEvent& damage_event)
{
    NET_Packet P;
    P.w_begin(M_COOP_DAMAGE_EVENT);
    P.w_u32(damage_event.timestamp);
    P.w_u32(damage_event.target_net_id);
    P.w_u32(damage_event.source_net_id);
    P.w_float(damage_event.damage);
    P.w_float(damage_event.impulse);
    P.w_float(damage_event.hit_x);
    P.w_float(damage_event.hit_y);
    P.w_float(damage_event.hit_z);
    P.w_float(damage_event.dir_x);
    P.w_float(damage_event.dir_y);
    P.w_float(damage_event.dir_z);
    P.w_u16(damage_event.hit_type);
    P.w_u16(damage_event.bone_id);
    
    CoopSessionManager::Instance().SendToAll(P, true);
}

bool CoopDamageHandler::ValidateFireEvent(const CoopFireEvent& fire_event, ClientID sender)
{
    // Check fire rate limiting
    if (!CheckFireRate(fire_event.shooter_net_id, fire_event.timestamp))
    {
        return false;
    }
    
    // Verify shooter exists
    ICoopNetEntity* shooter = CoopEntityRegistry::Instance().GetEntity(fire_event.shooter_net_id);
    if (!shooter)
    {
        return false;
    }
    
    // Verify direction is normalized
    Fvector dir;
    dir.set(fire_event.dir_x, fire_event.dir_y, fire_event.dir_z);
    float mag = dir.magnitude();
    if (mag < 0.9f || mag > 1.1f)
    {
        return false;
    }
    
    return true;
}

bool CoopDamageHandler::ValidateDamageEvent(const CoopDamageEvent& damage_event, ClientID sender)
{
    // Verify target exists
    ICoopNetEntity* target = CoopEntityRegistry::Instance().GetEntity(damage_event.target_net_id);
    if (!target)
    {
        return false;
    }
    
    // Verify source exists (if not environmental damage)
    if (damage_event.source_net_id != 0)
    {
        ICoopNetEntity* source = CoopEntityRegistry::Instance().GetEntity(damage_event.source_net_id);
        if (!source)
        {
            return false;
        }
    }
    
    // Verify damage is reasonable
    if (damage_event.damage < 0.0f || damage_event.damage > 1000.0f)
    {
        return false;
    }
    
    return true;
}

bool CoopDamageHandler::CheckFireRate(u32 shooter_net_id, u32 current_time)
{
    auto it = m_fire_rates.find(shooter_net_id);
    
    if (it == m_fire_rates.end())
    {
        // First fire from this player
        m_fire_rates[shooter_net_id] = { current_time, 1 };
        return true;
    }
    
    FireRateInfo& info = it->second;
    
    // Reset counter every second
    if (current_time - info.last_fire_time > 1000)
    {
        info.last_fire_time = current_time;
        info.fire_count = 1;
        return true;
    }
    
    // Allow up to 30 shots per second (generous for auto weapons)
    const u32 max_fire_rate = 30;
    if (info.fire_count >= max_fire_rate)
    {
        Msg("![COOP] Fire rate exceeded for player %u", shooter_net_id);
        return false;
    }
    
    info.fire_count++;
    return true;
}

} // namespace coop
