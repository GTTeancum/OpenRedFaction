#ifndef RF_SCENE_VEHICLE_EQUIPMENT_INCLUDED
#define RF_SCENE_VEHICLE_EQUIPMENT_INCLUDED
/* Include before campaign_combat_tick, after its weapon globals/helpers.
 * Call once after its frame0 initialization/duplicate-frame guard, before
 * on-foot input consumers: scene_vehicle_equipment_tick(stream,frame,on_foot).
 * Boarding stows the conventional handheld weapon like a weapon change:
 * cancel unspent burst/reload work, retain already committed ammunition and
 * live projectile owners. Cooldown ages normally while equipment is stowed.
 * Shared FP pair/f90 survive stowing; only the admitted continuous episode
 * stops. Source host selection supplies the later shared state observation. */
static struct {
    uint32_t occupied,release_mask,last_frame;
} scene_vehicle_equipment={0,0,UINT32_MAX};
/* boards,cancelled queued shots,cancelled reloads,cooldown ticks,
 * masked fire/alt frames,last frame,release mask,
 * boarding cooldown/remaining/delay/held/reload/shared charge deadline,
 * inventory hash before/after,post-cancel held/reload,current cooldown,occupied */
uint32_t rf_scene_vehicle_equipment[20];
static void scene_vehicle_equipment_board(scene_stream *s)
{
    uint32_t *p=rf_scene_vehicle_equipment;
    scene_player_riot_cancel();scene_continuous_charge_stop();
    ++p[0];p[1]+=combat_trigger.remaining;p[2]+=rf_scene_combat[6]!=0;
    p[8]=combat_trigger.cooldown;p[9]=combat_trigger.remaining;
    p[10]=combat_trigger.delay;p[11]=combat_trigger.held;
    p[12]=rf_scene_combat[6];p[13]=(uint32_t)scene_continuous_charge.due_ms;
    p[14]=npc_hash_bytes(2166136261u,&campaign_player_inventory,sizeof(campaign_player_inventory));
    combat_trigger.remaining=combat_trigger.delay=combat_trigger.held=0;
    /* Reload transfer occurs at completion. If Riot's earlier drain already
     * happened, keep that committed inventory as well: no refill or refund. */
    rf_scene_combat[6]=0;
    /* First on-foot draw requests idle, not the interrupted reload/fire pose. */
    s->player_pose_frame=0;s->player_slot=UINT32_MAX;
    s->player_shots=rf_scene_combat[0];s->player_reload=0;
    p[15]=npc_hash_bytes(2166136261u,&campaign_player_inventory,sizeof(campaign_player_inventory));
    p[16]=combat_trigger.held;p[17]=rf_scene_combat[6];
}
static void scene_vehicle_equipment_tick(scene_stream *s,uint32_t frame,uint32_t on_foot)
{
    uint32_t *p=rf_scene_vehicle_equipment;
    if(!s)return;
    if(!frame){
        memset(&scene_vehicle_equipment,0,sizeof(scene_vehicle_equipment));
        scene_vehicle_equipment.last_frame=UINT32_MAX;memset(p,0,sizeof(rf_scene_vehicle_equipment));
    }
    if(scene_vehicle_equipment.last_frame==frame)return;
    scene_vehicle_equipment.last_frame=frame;p[6]=frame;
    if(!on_foot){
        if(!scene_vehicle_equipment.occupied)scene_vehicle_equipment_board(s);
        scene_vehicle_equipment.occupied=1;
        if(combat_trigger.cooldown){--combat_trigger.cooldown;++p[3];}
        /* Vehicle controls remain untouched. Only a still-held control is
         * blocked when ownership returns to the on-foot handheld weapon. */
        scene_vehicle_equipment.release_mask=(player_input.fire?1u:0u)|(player_input.alt_fire?2u:0u);
    }else{
        scene_vehicle_equipment.occupied=0;
        if(!player_input.fire)scene_vehicle_equipment.release_mask&=~1u;
        if(!player_input.alt_fire)scene_vehicle_equipment.release_mask&=~2u;
        if(scene_vehicle_equipment.release_mask&1u){player_input.fire=0;++p[4];}
        if(scene_vehicle_equipment.release_mask&2u){player_input.alt_fire=0;++p[5];}
    }
    p[7]=scene_vehicle_equipment.release_mask;p[18]=combat_trigger.cooldown;
    p[19]=scene_vehicle_equipment.occupied;
}
#endif
