#ifndef RF_SCENE_TACTICAL_SERVICE_INCLUDED
#define RF_SCENE_TACTICAL_SERVICE_INCLUDED
/* New-phase admission is distinct from retained phase/recovery. Disarm and
 * unsupported current weapons do not rewrite an existing receipt. */
static int scene_tactical_weapon(const campaign_npc_body *o,
    const campaign_enemy_weapon_selection *selected)
{
    int32_t weapon;int status;
    if(!o||!selected)return RF_RANGE;
    weapon=o->view.weapons[0];
    if(!selected->primary||selected->melee||weapon<0||weapon>=64||
       !o->inventory.owned[weapon])return RF_NOT_FOUND;
    status=scene_blind_weapon(weapon);if(status)return status;
    /* The common portable fire path does not own original active-primary
     * bits. Never fabricate native stop callbacks for a foreign active owner. */
    for(uint32_t j=0;j<64;j++)if(o->firing.active[j])return RF_NOT_FOUND;
    return RF_OK;
}
static int scene_tactical_admit_phase(uint32_t slot,
    const campaign_enemy_weapon_selection *selected,int32_t now)
{
    campaign_npc_body *o;scene_tactical_receipt next;uint32_t target,seed;int status;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;
    if(o->tactical.flags&SCENE_TACTICAL_PRESENT)return RF_OK;
    if(!scene_tactical_service_allowed(o))return RF_OK;
    status=scene_tactical_weapon(o,selected);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_tactical_owner(slot);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    target=scene_blind_target_handle(o);
    if(!target||target==o->registration.handle||!scene_blind_target_live(target))return RF_OK;
    status=scene_patrol_resources(slot,&o->selection,0);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    /* Private episode seed, independent of weapon spread and global CRT. */
    seed=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    seed=(seed^o->registration.handle)*16777619u;
    seed=(seed^campaign_seeds.items[slot].class_index)*16777619u;
    next=o->tactical;
    status=scene_tactical_receipt_begin(&next,target,o->combat_scripted,seed,now);if(status)return status;
    o->tactical=next;return RF_OK;
}
/* Called after fallible command cancellation, before the assignment-only
 * Attack publisher. Existing phase RNG and independent recovery survive a
 * repeated explicit order; its stationary interval is deliberately restarted. */
static int scene_tactical_attack_prepare(uint32_t slot,uint32_t target,int32_t now,
    scene_tactical_receipt *next)
{
    const campaign_npc_body *o;int status;uint32_t seed;
    campaign_enemy_weapon_selection selected={0};
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,campaign_shotgun_id,
        campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    if(!next||!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;*next=o->tactical;
    status=scene_tactical_receipt_admit(next);if(status)return status;
    status=scene_tactical_owner(slot);
    if(status==RF_NOT_FOUND){scene_tactical_receipt_release(next);return RF_OK;}
    if(status)return status;
    if(!target||target==o->registration.handle||!scene_blind_target_live(target)){
        scene_tactical_receipt_release(next);return RF_OK;
    }
    if(!(next->flags&SCENE_TACTICAL_PRESENT)){
        status=campaign_enemy_weapon_select(o->view.weapons[0],ids,campaign_primary,
            campaign_weapon_supply.definitions,campaign_weapon_supply.names.count,&selected);
        if(status==RF_NOT_FOUND)status=campaign_enemy_extra_weapon_select(o->view.weapons[0],&campaign_weapon_supply,&selected);
        if(status==RF_NOT_FOUND)status=campaign_enemy_flame_select(o,&campaign_weapon_supply,&selected);
        if(status==RF_NOT_FOUND)status=campaign_enemy_fusion_select(o,&campaign_weapon_supply,&selected);
        if(status==RF_NOT_FOUND&&o->view.weapons[0]>=0&&o->view.weapons[0]<64&&
           (uint32_t)o->view.weapons[0]<campaign_weapon_supply.names.count){
            if(o->view.weapons[0]==campaign_grenade_id&&scene_grenade_resources){
                selected.primary=campaign_primary+5;selected.slot=5;status=RF_OK;
            }else if(o->view.weapons[0]==campaign_rocket_id&&scene_rocket_resources){
                selected.primary=campaign_primary+4;selected.slot=4;
                selected.ammo=campaign_weapon_supply.definitions+o->view.weapons[0];status=RF_OK;
            }
        }
        if(status==RF_NOT_FOUND){scene_tactical_receipt_release(next);return RF_OK;}
        if(status)return status;
        status=scene_tactical_weapon(o,&selected);
        if(status==RF_NOT_FOUND||o->view.weapons[0]==campaign_riot_id){scene_tactical_receipt_release(next);return RF_OK;}
        if(status)return status;
    }
    status=scene_patrol_resources(slot,&o->selection,0);
    if(status==RF_NOT_FOUND){scene_tactical_receipt_release(next);return RF_OK;}
    if(status)return status;
    seed=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    seed=(seed^o->registration.handle)*16777619u;
    seed=(seed^campaign_seeds.items[slot].class_index)*16777619u;
    return scene_tactical_receipt_begin(next,target,1,seed,now);
}
/* Every callback boundary is closed before writing a retry or publishing a
 * route, including benign resource misses. Never dereference a stale owner. */
static int scene_tactical_service_recheck(campaign_npc_body *owners,uint32_t slot,
    uint32_t handle,uint32_t target,int32_t weapon,const scene_tactical_receipt *before)
{
    campaign_npc_body *o;
    if(owners!=campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_FORMAT;
    o=owners+slot;
    if(o->registration.handle!=handle||o->view.weapons[0]!=weapon||scene_npc_reload_identity(slot,o)||
       memcmp(&o->tactical,before,sizeof(*before))||
       scene_blind_target_handle(o)!=target||!scene_blind_target_live(target)||
       !scene_tactical_service_allowed(o))return RF_FORMAT;
    return RF_OK;
}
/* Only the preselected fairness slot may reach route work this frame. The
 * selection budget is not replenished when later range/LOS admission fails. */
static int scene_tactical_try_route(scene_stream *stream,uint32_t slot,uint32_t chosen,
    const campaign_enemy_weapon_selection *selected,uint32_t frame,
    float distance2,float range,const float target_position[3],const float sight_delta[3])
{
    campaign_npc_body *owners=campaign_npc_bodies,*o;scene_tactical_receipt next,before;
    scene_tactical_route_candidate candidate;uint32_t handle,target,blocked,visible,found;int status,expired;int32_t weapon;
    int32_t now=scene_patrol_now(frame);
    if(slot!=chosen)return RF_OK;
    if(!stream||!owners||slot>=campaign_npc_body_count||!target_position||!sight_delta)return RF_RANGE;
    o=owners+slot;
    if(!(o->tactical.flags&SCENE_TACTICAL_PRESENT)||o->tactical.phase!=SCENE_TACTICAL_STATIONARY||
       !scene_tactical_service_allowed(o)||campaign_modes[o->movement_slot].index!=1)return RF_OK;
    status=rf_timer_expired(o->tactical.phase_due,now,&expired);if(status)return status;if(!expired)return RF_OK;
    weapon=o->view.weapons[0];
    status=scene_tactical_weapon(o,selected);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(!isfinite(distance2)||distance2<0||!isfinite(range)||range<=0)return RF_FORMAT;
    if((double)distance2>(double)range*range)return RF_OK;
    handle=o->registration.handle;target=o->tactical.target;
    if(target!=scene_blind_target_handle(o)||!scene_blind_target_live(target))return RF_OK;
    before=o->tactical;
    status=combat_obstructed(stream,o->eye_position,sight_delta,&blocked);if(status)return status;
    status=scene_tactical_service_recheck(owners,slot,handle,target,weapon,&before);if(status)return status;
    if(blocked)return RF_OK;
    next=before;
    if(target==campaign_player_object.handle){
        status=scene_tactical_target_facing(scene_actor_body.state.position,actor_look.eye_orientation+6,
            campaign_player_geometry.sight_cosine,o->body.state.position,&visible);
        if(status)return status;
        if(!visible){status=scene_tactical_retry(&next,now);if(status)return status;o->tactical=next;return RF_OK;}
    }
    status=scene_patrol_resources(slot,&o->selection,1);
    if(status&&status!=RF_NOT_FOUND)return status;
    {int closed=scene_tactical_service_recheck(owners,slot,handle,target,weapon,&before);if(closed)return closed;}
    if(status==RF_NOT_FOUND){status=scene_tactical_retry(&next,now);if(status)return status;o->tactical=next;return RF_OK;}
    status=scene_tactical_navigation(stream,slot,target_position,&next.random,&candidate,&found);
    if(status==RF_NOT_FOUND)found=0;else if(status)return status;
    status=scene_tactical_service_recheck(owners,slot,handle,target,weapon,&before);if(status)return status;
    if(!found){status=scene_tactical_retry(&next,now);if(status)return status;o->tactical=next;return RF_OK;}
    return scene_tactical_route_publish(slot,handle,target,&candidate,&next.random,now);
}
#endif
