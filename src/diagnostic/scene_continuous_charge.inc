#ifndef RF_SCENE_CONTINUOUS_CHARGE_INCLUDED
#define RF_SCENE_CONTINUOUS_CHARGE_INCLUDED
/* Source4aa230/4aa560/4aaf77. This is a gameplay receipt, not a view handle.
 * Explicit port policies: fixed60 Hz observation plus committed selection;
 * snapshot after ordinary selection before later port contact callbacks;
 * nonnegative attempt saturation, committed before contact callbacks. */
enum {SCENE_CONTINUOUS_PROFILES=19,SCENE_CONTINUOUS_HOSTS=8};
static struct {
    rf_weapon_fp_state_definition definition[SCENE_CONTINUOUS_PROFILES];
    uint32_t valid[SCENE_CONTINUOUS_PROFILES],ready;
    int32_t weapons[SCENE_CONTINUOUS_PROFILES],hosts[SCENE_CONTINUOUS_HOSTS];
} scene_continuous_charge_resources;
static int scene_continuous_charge_resources_open(rf_vpp *tables,rf_vpp *meshes,rf_vpp *motions)
{
    static const char *const hosts[SCENE_CONTINUOUS_HOSTS]={"Vauss","HEAP","Torpedo","APC Minigun",
        "Fighter Minigun","Drill","Fighter Rocket","APC Rocket"};
    uint32_t i;int status;rf_weapon_fp_state_definition definition;
    memset(&scene_continuous_charge_resources,0,sizeof(scene_continuous_charge_resources));
    for(i=0;i<SCENE_CONTINUOUS_PROFILES;i++) {
        const char *name=i<18?campaign_weapon_names[i]:"Jeep Gun";
        status=rf_weapon_fp_state_load(tables,name,128*1024,&definition);if(status)return status;
        status=rf_player_weapon_state_validate(meshes,motions,&definition,64*1024,
            scene_continuous_charge_resources.valid+i);if(status)return status;
        if(!definition.state_count || scene_continuous_charge_resources.valid[i]!=(1u<<definition.state_count)-1u)return RF_FORMAT;
        scene_continuous_charge_resources.definition[i]=definition;
        scene_continuous_charge_resources.weapons[i]=rf_weapon_name_find(&campaign_weapon_supply.names,name);
        if(scene_continuous_charge_resources.weapons[i]<0)return RF_FORMAT;
    }
    for(i=0;i<SCENE_CONTINUOUS_HOSTS;i++) {
        status=rf_weapon_fp_state_load(tables,hosts[i],128*1024,&definition);if(status)return status;
        if(definition.mesh[0] || definition.state_count)return RF_FORMAT;
        scene_continuous_charge_resources.hosts[i]=rf_weapon_name_find(&campaign_weapon_supply.names,hosts[i]);
        if(scene_continuous_charge_resources.hosts[i]<0)return RF_FORMAT;
    }
    scene_continuous_charge_resources.ready=1;return RF_OK;
}
static int scene_continuous_charge_profile_admit(const scene_stream *s,int32_t profile)
{
    if(!s || profile< -1 || profile>=SCENE_CONTINUOUS_PROFILES)return RF_RANGE;
    if(profile==-1)return RF_OK;
    if(!scene_continuous_charge_resources.ready)return RF_NOT_FOUND;
    const rf_weapon_fp_state_definition *d=scene_continuous_charge_resources.definition+profile;
    return d->mesh[0] && d->state_count &&
        scene_continuous_charge_resources.valid[profile]==(1u<<d->state_count)-1u?RF_OK:RF_FORMAT;
}
static int scene_continuous_charge_live_admit(void)
{
    return scene_continuous_charge_runtime.epoch &&
        scene_continuous_charge_runtime.source==campaign_player_object.handle &&
        campaign_player_object.view==&campaign_player_view &&
        rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view?RF_OK:RF_NOT_FOUND;
}
static uint32_t scene_continuous_charge_busy(void)
{return scene_continuous_charge_runtime.busy || scene_continuous_charge_runtime.suspended ||
    (scene_continuous_charge_runtime.prepared && scene_continuous_charge_runtime.prepared_frame==combat_frame &&
     (!scene_continuous_charge_runtime.serviced || scene_continuous_charge_runtime.serviced_frame!=combat_frame));}
static uint32_t scene_continuous_charge_is_constructor(void)
{
    const scene_continuous_charge_state initial={-1,-1,0,0,-1,-1,-1,0};
    return !memcmp(&scene_continuous_charge,&initial,sizeof(initial));
}
static int32_t scene_continuous_charge_now(void)
{return scene_riot_primary_now(combat_frame==UINT32_MAX?0:combat_frame);}
static void scene_continuous_charge_restore_scope(uint32_t active)
{scene_continuous_charge_runtime.suspended=!!active;}
static void scene_continuous_charge_restore_publish(const scene_continuous_charge_state *value)
{
    uint64_t epoch=scene_continuous_charge_runtime.epoch+1;
    scene_continuous_charge=*value;
    memset(&scene_continuous_charge_runtime,0,sizeof(scene_continuous_charge_runtime));
    scene_continuous_charge_runtime.epoch=epoch?epoch:1;
    scene_continuous_charge_runtime.source=campaign_player_object.handle;
    scene_continuous_charge_runtime.debit_weapon=-1;
}
static void scene_continuous_charge_reset_player(void)
{
    const scene_continuous_charge_state initial={-1,-1,0,0,-1,-1,-1,0};
    if(scene_continuous_charge_runtime.suspended)return;
    scene_continuous_charge_restore_publish(&initial);
}
static void scene_continuous_charge_stop(void)
{
    if(scene_continuous_charge_runtime.suspended)return;
    if(scene_continuous_charge.episode_weapon>=0)++rf_scene_continuous_charge[4];
    scene_continuous_charge.episode_weapon=-1;scene_continuous_charge.episode_mode=0;
}
/* Selected descriptor is the supported linked host's real primary while
 * occupied. Inventory's stowed equipped slot is never a mounted descriptor. */
static int scene_continuous_charge_selection(const scene_stream *s,int32_t *weapon,int32_t *profile)
{
    uint32_t i,mounted=0;int32_t value=-1;int status;
    if(!s || !weapon || !profile)return RF_RANGE;
    status=scene_continuous_charge_host_weapon(s,&value,&mounted);if(status)return status;
    if(!mounted && !campaign_explicit_unarmed && campaign_equipped_slot<18 &&
        campaign_player_inventory.owned[campaign_slot_weapon(campaign_equipped_slot)]) {
        uint32_t slot=campaign_view_slot();
        if(slot>=18)return RF_RANGE;
        value=scene_continuous_charge_resources.weapons[slot];
    }
    *weapon=value;*profile=-1;
    if(value<0)return RF_OK;
    if(!scene_continuous_charge_resources.ready)return RF_NOT_FOUND;
    for(i=0;i<SCENE_CONTINUOUS_PROFILES;i++)if(value==scene_continuous_charge_resources.weapons[i]) {
        *profile=(int32_t)i;return scene_continuous_charge_profile_admit(s,(int32_t)i);
    }
    for(i=0;i<SCENE_CONTINUOUS_HOSTS;i++)if(value==scene_continuous_charge_resources.hosts[i]) {
        *profile=-2;return RF_OK; /* Real empty-FP descriptor preserves binding. */
    }
    return RF_NOT_FOUND;
}
static uint32_t scene_continuous_charge_has(int32_t state)
{return scene_continuous_charge.current==state || scene_continuous_charge.pending==state;}
static void scene_continuous_charge_request(int32_t profile,int32_t state)
{
    scene_continuous_charge_state *s=&scene_continuous_charge;float progress;
    if(profile<0 || profile>=SCENE_CONTINUOUS_PROFILES || state<0 ||
       (uint32_t)state>=scene_continuous_charge_resources.definition[profile].state_count)return;
    if(!s->duration){s->pending=state;s->duration=.2f;s->elapsed=0;return;}
    progress=s->elapsed/s->duration;
    if(progress>.5f){s->current=s->pending;s->elapsed=(1.0f-progress)*.2f;}
    else s->elapsed=progress*.2f;
    s->pending=state;s->duration=.2f;
}
static int scene_continuous_charge_observe(scene_stream *stream,uint32_t frame)
{
    scene_continuous_charge_state *s=&scene_continuous_charge;
    int32_t weapon,profile;float command[3];int status;
    status=scene_continuous_charge_selection(stream,&weapon,&profile);if(status)return status;
    /*4a5910's raw query skips binding -1; ordinary4aa6d0 skips the updater.
     * Neither query manufactures a model clear or resets the retained pair. */
    if(weapon<0)return RF_OK;
    if(profile>=0){s->last_profile=profile;++rf_scene_continuous_charge[6];}
    if(s->last_profile<0)return RF_OK;
    /*4aa3d3 queries the PLAYER actor active byte, even when4a5910 selected
     * a linked host weapon. Mounted425830/41a870 owns the HOST byte instead;
     * Jeep continuous flags therefore do not create a player episode. */
    if(s->episode_weapon==weapon) {
        if(!scene_continuous_charge_has(2))scene_continuous_charge_request(profile,2);
    }else {
        if(!scene_continuous_charge_has(0))scene_continuous_charge_request(profile,0);
        actor_command(frame,command);
        if((command[0]!=0 || command[1]!=0 || command[2]!=0) &&
           ((rf_scene_actor_landing[1]<16 && campaign_modes[rf_scene_actor_landing[1]].index==1 &&
             rf_scene_actor_movement_settings.mode==1) || (rf_scene_actor_stance_flags&0x400u)) &&
           !scene_continuous_charge_has(1)) {
            /* Preserve the second request0: it can reverse/rescale a blend.
             * Zero-state host requests all fail, so omitted pure queries have
             * no mutation there either. Jeep has actual idle0, no run state. */
            scene_continuous_charge_request(profile,
                command[0]<-.1f || command[0]>.1f || command[2]<-.1f || command[2]>.1f?1:0);
        }
    }
    if(s->duration>0) {
        s->elapsed+=scene_step_seconds;
        if(s->elapsed>=s->duration){s->current=s->pending;s->pending=-1;s->duration=0;}
    }
    return RF_OK;
}
static void scene_continuous_charge_selected(void)
{
    if(scene_continuous_charge_runtime.suspended)return;
    ++rf_scene_continuous_charge[5];
    if(scene_continuous_charge_live_admit() || !scene_continuous_charge_resources.ready ||
       !scene_actor_collision_owner)return;
    /* Committed ordinary selection has its own source choose/blend call;
     * it never invokes periodic debit. Full reset is only constructor policy. */
    (void)scene_continuous_charge_observe(scene_actor_collision_owner,combat_frame==UINT32_MAX?0:combat_frame);
}
static void scene_continuous_charge_accept(int32_t weapon,uint32_t mode)
{
    int32_t profile;uint32_t i;
    if(scene_continuous_charge_runtime.suspended || scene_continuous_charge_live_admit() ||
       campaign_player_damage.state.effects.health<=0 || (campaign_player_view.flags_7c&(2u|0x4000u)) ||
       (mode!=0 && mode!=0x20u))return;
    for(i=0;i<18;i++)if(weapon==scene_continuous_charge_resources.weapons[i])break;
    if(i==18 || !scene_continuous_charge_resources.ready)return;
    profile=(int32_t)i;
    const rf_weapon_fp_state_definition *d=scene_continuous_charge_resources.definition+profile;
    if(!(mode?d->alternate_continuous:d->primary_continuous))return;
    /*41a8db..913 retains the onset mode while active. Repeated port
     * cooldown admissions do not restart or remode a source episode. */
    if(scene_continuous_charge.episode_weapon==weapon)return;
    scene_continuous_charge.episode_weapon=weapon;scene_continuous_charge.episode_mode=mode;
}
static int scene_continuous_charge_prepare(scene_stream *stream,uint32_t frame)
{
    int32_t weapon,profile;int status;
    if(scene_continuous_charge_runtime.suspended)return RF_OK;
    if(scene_continuous_charge_live_admit())return RF_NOT_FOUND;
    status=scene_continuous_charge_selection(stream,&weapon,&profile);if(status)return status;
    scene_continuous_charge_runtime.prepared=1;scene_continuous_charge_runtime.prepared_frame=frame;
    scene_continuous_charge_runtime.prepared_source=campaign_player_object.handle;
    scene_continuous_charge_runtime.prepared_epoch=scene_continuous_charge_runtime.epoch;
    scene_continuous_charge_runtime.debit_weapon=weapon;
    if(scene_continuous_charge.episode_weapon>=0) {
        uint32_t held=scene_continuous_charge.episode_mode?player_input.alt_fire:player_input.fire;
        if(weapon!=scene_continuous_charge.episode_weapon ||
           profile<0 || profile==18 || !held || scene_driller_active(stream) || scene_turret_player_active() ||
           scene_player_jeep_gunner_active(stream) || campaign_player_damage.state.effects.health<=0 ||
           rf_scene_enemy_combat[6] || rf_scene_combat[6] ||
           (campaign_equipped_slot==13 && campaign_machine_mode.pending) ||
           weapon<0 || weapon>=64 || campaign_player_inventory.loaded[weapon]<=0)
            scene_continuous_charge_stop();
    }
    return RF_OK;
}
static void scene_continuous_charge_telemetry(uint32_t frame)
{
    const scene_continuous_charge_state *s=&scene_continuous_charge;
    rf_scene_continuous_charge[7]=frame;rf_scene_continuous_charge[8]=(uint32_t)s->current;
    rf_scene_continuous_charge[9]=(uint32_t)s->pending;rf_scene_continuous_charge[10]=(uint32_t)s->due_ms;
    rf_scene_continuous_charge[11]=(uint32_t)s->last_profile;rf_scene_continuous_charge[12]=(uint32_t)s->episode_weapon;
    rf_scene_continuous_charge[13]=s->episode_mode;
}
static int scene_continuous_charge_finish(scene_stream *stream,uint32_t frame,scene_weapon_empty_request *empty_request)
{
    int32_t weapon,interval;int expired,status;uint32_t before;
    if(scene_continuous_charge_runtime.suspended || scene_continuous_charge_runtime.busy || !scene_continuous_charge_runtime.prepared ||
       scene_continuous_charge_runtime.prepared_frame!=frame ||
       (scene_continuous_charge_runtime.serviced && scene_continuous_charge_runtime.serviced_frame==frame))return RF_OK;
    if(scene_continuous_charge_live_admit() ||
       scene_continuous_charge_runtime.prepared_source!=campaign_player_object.handle ||
       scene_continuous_charge_runtime.prepared_epoch!=scene_continuous_charge_runtime.epoch)return RF_OK;
    scene_continuous_charge_runtime.serviced=1;scene_continuous_charge_runtime.serviced_frame=frame;
    scene_continuous_charge_runtime.busy=1;
    status=scene_continuous_charge_observe(stream,frame);if(status)goto done;
    weapon=scene_continuous_charge_runtime.debit_weapon;
    if(campaign_player_damage.state.effects.health<=0 || (campaign_player_view.flags_7c&(2u|0x4000u)) ||
       !scene_continuous_charge_has(2) || (weapon!=campaign_riot_id && weapon!=campaign_flame_id))goto done;
    status=rf_timer_expired(scene_continuous_charge.due_ms,scene_continuous_charge_now(),&expired);if(status)goto done;
    if(scene_continuous_charge.due_ms!=-1 && !expired)goto done;
    {const rf_weapon_primary_definition *definition=campaign_primary+(weapon==campaign_riot_id?2:10);
     if(!definition->magazine || !isfinite(definition->drain_seconds) || definition->drain_seconds<=0){status=RF_FORMAT;goto done;}
     interval=(int32_t)(definition->drain_seconds/(float)definition->magazine*1000.0f);
     if(interval!=(weapon==campaign_riot_id?25:20)){status=RF_FORMAT;goto done;}}
    before=campaign_player_inventory.loaded[weapon]>0?1u:0u;
    if(before)--campaign_player_inventory.loaded[weapon];
    else campaign_player_inventory.loaded[weapon]=0;
    status=rf_timer_set(&scene_continuous_charge.due_ms,scene_continuous_charge_now(),interval);if(status)goto done;
    ++rf_scene_continuous_charge[0];rf_scene_continuous_charge[1]+=before;
    if(weapon==campaign_riot_id)rf_scene_riot[2]+=before;
    if(before) {
        if(weapon==campaign_flame_id && empty_request)
            *empty_request=(scene_weapon_empty_request){campaign_player_object.handle,10,weapon,SCENE_WEAPON_EMPTY_FUEL_DEBIT};
        campaign_ammo_publish();
    }
done:
    scene_continuous_charge_runtime.busy=0;scene_continuous_charge_telemetry(frame);return status;
}
static void scene_continuous_charge_attempt(uint32_t source,int32_t weapon)
{
    /* Original42c310 bit8 gate is separate from periodic4257c0. Saturating
     * and committing before port contact callbacks keeps RFPL nonnegative and
     * cannot charge a replacement player/load. Accepted contact still runs. */
    if(scene_continuous_charge_runtime.suspended || scene_continuous_charge_live_admit() || source!=campaign_player_object.handle ||
       weapon!=campaign_riot_id || weapon<0 || weapon>=64 || !(campaign_player_view.flags_7c&8u))return;
    ++rf_scene_continuous_charge[2];
    if(campaign_player_inventory.loaded[weapon]>0){--campaign_player_inventory.loaded[weapon];++rf_scene_continuous_charge[3];campaign_ammo_publish();}
}
#endif
