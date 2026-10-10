#ifndef RF_SCENE_WEAPON_CAMERA_FEEDBACK_INCLUDED
#define RF_SCENE_WEAPON_CAMERA_FEEDBACK_INCLUDED

/* Original4258e9..4258ff captures the firing descriptor;426c32 may select
 * another weapon.426c3a..426c7e reuses it, checks+264 bit0x20000000
 * and calls40e0b0 with+4e0/+4e4.
 * The presence-only camera block is separate from canonical reset flags.
 * Scope: accepted on-foot AR/MP and player-operated stationary Vauss only.
 * Autonomous/NPC turrets, HEAP and other vehicle guns remain outside it.
 * Include after both frame-local empty-selection consumers. */
typedef struct scene_weapon_camera_profile {
    int32_t weapon;rf_weapon_camera_shake authored;
} scene_weapon_camera_profile;
static scene_weapon_camera_profile scene_weapon_camera_profiles[3];
static const uint32_t scene_weapon_camera_views[3]={1,13,17};
static scene_weapon_camera_profile scene_mounted_vauss_camera_profile;

/* Immutable optional metadata, loaded before gameplay while tables is open.
 * No per-shot archive I/O/allocation. Any missing/bad record is independently
 * disabled; it cannot reject weapon startup or alter catalog/save hashes. */
static void scene_weapon_camera_load(rf_vpp *tables)
{
    uint32_t i;
    memset(scene_weapon_camera_profiles,0,sizeof(scene_weapon_camera_profiles));
    memset(&scene_mounted_vauss_camera_profile,0,sizeof(scene_mounted_vauss_camera_profile));
    scene_mounted_vauss_camera_profile.weapon=-1;
    for(i=0;i<3;i++)scene_weapon_camera_profiles[i].weapon=-1;
    if(!tables || campaign_weapon_supply.names.count>64 ||
       campaign_weapon_supply.names.primary_count>campaign_weapon_supply.names.count)return;
    for(i=0;i<3;i++){
        const char *name=campaign_weapon_names[scene_weapon_camera_views[i]];
        rf_weapon_camera_shake authored={0};
        int32_t weapon=rf_weapon_name_find(&campaign_weapon_supply.names,name);
        if(weapon<0 || (uint32_t)weapon>=campaign_weapon_supply.names.primary_count ||
           rf_weapon_camera_shake_load(tables,name,128*1024,&authored) || !authored.enabled ||
           (double)authored.seconds*1000.0>(double)RF_TIMER_PERIOD)continue;
        scene_weapon_camera_profiles[i].weapon=weapon;
        scene_weapon_camera_profiles[i].authored=authored;
    }
    /* Exact Vauss metadata is independent of handheld views and sound aliases.
     * Installed values are .001/.04; pass authored values through unchanged. */
    {
        rf_weapon_camera_shake authored={0};
        int32_t weapon=rf_weapon_name_find(&campaign_weapon_supply.names,"Vauss");
        if(weapon>=0 && (uint32_t)weapon<campaign_weapon_supply.names.primary_count &&
           !rf_weapon_camera_shake_load(tables,"Vauss",128*1024,&authored) && authored.enabled &&
           (double)authored.seconds*1000.0<=(double)RF_TIMER_PERIOD){
            scene_mounted_vauss_camera_profile.weapon=weapon;
            scene_mounted_vauss_camera_profile.authored=authored;
        }
    }
}

/* Only the player's successful stationary shot dispatch calls this, after
 * hearing succeeds. Requalify captured full identities after those callbacks;
 * never infer acceptance from input, counters or the current weapon selection.
 * Existing turret control fires before the sole camera apply, so this effect is
 * sampled in that same view pass. It does not perturb the mounted shot basis. */
static void scene_weapon_camera_after_mounted_shot(scene_stream *s,uint32_t frame,
    uint32_t player,uint32_t host,int32_t weapon)
{
    const scene_weapon_camera_profile *profile=&scene_mounted_vauss_camera_profile;
    scene_turret_owner *o;uint64_t elapsed;int32_t now;
    if(!s || !player || player==UINT32_MAX || !host || host==UINT32_MAX ||
       weapon<0 || weapon>=64 || weapon!=scene_turret_vauss_id ||
       profile->weapon!=weapon || !profile->authored.enabled ||
       campaign_weapon_supply.names.count>64 ||
       (uint32_t)weapon>=campaign_weapon_supply.names.primary_count ||
       rf_weapon_name_find(&campaign_weapon_supply.names,"Vauss")!=weapon ||
       player!=scene_turret_player.player || player!=campaign_player_object.handle ||
       !scene_turret_player_controls(host) ||
       campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=player || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=player ||
       rf_object_registry_lookup(&campaign_registry,player)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)player)!=&campaign_player_view ||
       !scene_actor_body.allocated_bytes ||
       ((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) ||
       ((campaign_player_view.flags_810|campaign_player_damage.state.effects.flags_810)&1u) ||
       !isfinite(campaign_player_damage.state.effects.health) ||
       !(campaign_player_damage.state.effects.health>0) ||
       scene_driller_active(s) || s->apc_aim_active || campaign_cutscene_runtime.active)return;
    o=scene_turret_lookup(host);
    if(!o || (uint32_t)(o-scene_turrets)>=scene_turret_authored_count ||
       !campaign_seeds.classes || o->class_index>=campaign_seeds.class_count ||
       campaign_seeds.classes[o->class_index].physics.use_kind!=4 ||
       scene_turret_operator_find(host) || scene_turret_generated_find(host) ||
       (uint32_t)o->view.handle!=host || o->view.type!=0 || o->damage.effects.handle!=host ||
       o->dead || (o->view.flags_7c&(2u|0x4000u)) ||
       ((o->view.flags_810|o->damage.effects.flags_810)&1u) ||
       !isfinite(o->damage.effects.health) || !(o->damage.effects.health>0))return;
    elapsed=(uint64_t)frame*1000/60;
    now=(int32_t)(elapsed?((elapsed-1)%RF_TIMER_PERIOD)+1:0);
    /* Optional feedback must never turn an accepted shot into failure. */
    (void)rf_scene_player_feedback(player,profile->authored.strength,profile->authored.seconds,now);
}

/* Call once after combat and applicable empty-selection consumers succeed.
 * SHOT is recorded immediately after actual ammo debit, even with rounds
 * remaining; it is neither held input nor a pellet/counter delta. Repeated
 * rendering of one frame receives no new receipt from campaign_combat_tick.
 * Never reread current selection/mode/ammo/ownership here: final-shot auto
 * selection can change them, and MP Special intentionally is not owned. */
static void scene_weapon_camera_after_combat(scene_stream *s,uint32_t frame,
    const scene_weapon_empty_request *request)
{
    uint32_t i;uint64_t elapsed;int32_t now;
    if(!s || !request || request->operation!=SCENE_WEAPON_EMPTY_SHOT ||
       (request->slot!=1 && request->slot!=13) || request->weapon<0 || request->weapon>=64 ||
       !request->source || request->source==UINT32_MAX ||
       request->source!=campaign_player_object.handle ||
       campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=request->source || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=request->source ||
       rf_object_registry_lookup(&campaign_registry,request->source)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)request->source)!=&campaign_player_view ||
       !scene_actor_body.allocated_bytes ||
       ((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) ||
       !isfinite(campaign_player_damage.state.effects.health) ||
       !(campaign_player_damage.state.effects.health>0) || rf_scene_enemy_combat[6] ||
       scene_driller_active(s) || scene_turret_player_active() || s->apc_aim_active ||
       campaign_cutscene_runtime.active || campaign_player_form.active ||
       campaign_weapon_supply.names.count>64 ||
       (uint32_t)request->weapon>=campaign_weapon_supply.names.primary_count ||
       request->slot>=scene_weapon_slots() || !scene_weapon_available(request->slot))return;
    for(i=0;i<3;i++){
        const scene_weapon_camera_profile *profile=scene_weapon_camera_profiles+i;
        uint32_t view=scene_weapon_camera_views[i];
        if(profile->weapon!=request->weapon || !profile->authored.enabled ||
           request->slot!=(i?13u:1u) || !s->player_weapon[view] ||
           !s->player_weapon[view]->bone_count ||
           rf_weapon_name_find(&campaign_weapon_supply.names,campaign_weapon_names[view])!=request->weapon)
            continue;
        elapsed=(uint64_t)frame*1000/60;
        now=(int32_t)(elapsed?((elapsed-1)%RF_TIMER_PERIOD)+1:0);
        /* Existing camera owner replaces, never accumulates. This optional
         * cosmetic request cannot turn an already accepted shot into failure. */
        (void)rf_scene_player_feedback(request->source,profile->authored.strength,
            profile->authored.seconds,now);
        return;
    }
}
#endif
