#ifndef RF_SCENE_PLAYER_HOLSTER_INPUT_INCLUDED
#define RF_SCENE_PLAYER_HOLSTER_INPUT_INCLUDED
/* Xbox first-playable policy around the recovered posture bit. A fresh click
 * toggles settled equipment; busy work declines, with no queued toggle and no
 * cancellation/refund. Fire/alternate/reload require drawing first. Manual
 * cycle draws the chosen owned weapon. Released world projectiles keep ticking.
 * Include after all handheld input owners and before combat input. */
static void scene_player_holster_mask(void)
{
    if(scene_player_holster_release&1u)player_input.fire=0;
    if(scene_player_holster_release&2u)player_input.alt_fire=0;
    if(scene_player_holster_release&4u)player_input.reload=0;
    if(scene_player_weapon_holstered())player_input.fire=player_input.alt_fire=player_input.reload=0;
}
static uint32_t scene_player_holster_owner(const scene_stream *s,uint32_t on_foot)
{
    uint32_t source=campaign_player_object.handle,slot=campaign_equipped_slot;
    int32_t weapon;
    if(!s || !on_foot || campaign_explicit_unarmed || slot>=SCENE_WEAPON_SLOTS ||
       campaign_cutscene_runtime.active || campaign_defuse.active || campaign_endgame.phase ||
       !source || source==UINT32_MAX || campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=source || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=source || campaign_player_view.linked_handle!=-1 ||
       rf_object_registry_lookup(&campaign_registry,source)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)source)!=&campaign_player_view ||
       !scene_actor_body.allocated_bytes ||
       ((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) ||
       (campaign_player_view.flags_810&1u) || !isfinite(campaign_player_damage.state.effects.health) ||
       campaign_player_damage.state.effects.health<=0 || !scene_weapon_available(slot))return 0;
    weapon=campaign_slot_weapon(slot);
    return weapon>=0 && weapon<64 && campaign_player_inventory.owned[weapon] &&
        campaign_view_slot()<SCENE_WEAPON_SLOTS && s->player_weapon[campaign_view_slot()]!=NULL;
}
static uint32_t scene_player_holster_busy(const scene_stream *s)
{
    /* Preserve accepted reload transfer, throws, delayed contacts, mode/tag
     * ownership and the represented shared FP2/f90 tail. Independent flights
     * and ordinary cooldowns are deliberately not an admission blocker. */
    return rf_scene_combat[6] || combat_trigger.remaining || combat_trigger.delay ||
        campaign_machine_mode.pending || (s->undercover && s->undercover->mode.pending) ||
        scene_grenade_throw.phase || scene_grenade_throw.ticks || scene_remote_holster_pending() ||
        scene_flame_input_pending() || scene_flame_canister_pending || scene_fusion_input.reloading ||
        scene_player_shield_bash_state.pending || scene_player_riot_save_pending() ||
        s->rocket_lock.active || scene_continuous_charge_busy() ||
        scene_continuous_charge.episode_weapon>=0 || scene_continuous_charge_has(2);
}
static void scene_player_holster_input(scene_stream *s,uint32_t frame,uint32_t on_foot)
{
    uint32_t controls=scene_player_holster_controls();
    uint32_t edge=player_input.holster && !scene_player_holster_held;
    if(!frame){scene_player_holster_held=scene_player_holster_raw;scene_player_holster_release|=controls;edge=0;}
    scene_player_holster_held=scene_player_holster_raw;
    scene_player_holster_release&=controls;
    /* Mounted controls are owned elsewhere. A click there is consumed, never
     * queued for dismount; retained handheld posture survives the vehicle. */
    if(!on_foot)return;
    if(edge && scene_player_holster_owner(s,on_foot) && !scene_player_holster_busy(s)){
        scene_player_holster_set(!scene_player_weapon_holstered());
        scene_player_holster_release|=controls;
        scene_scope_owner_reset(!!player_input.alt_fire);scene_scanner_enabled=0;
        scene_weapon_empty_audio_forget();scene_machine_pistol_audio_close();scene_hmg_audio_close();
        scene_rifle_audio_stop(scene_actor_body.state.position);scene_weapon_loop_audio_close();
        /* Idle is the existing first-playable presentation boundary. This is
         * not an invented retail draw animation or a fresh firing request. */
        s->player_pose_frame=0;s->player_slot=UINT32_MAX;
        s->player_shots=rf_scene_combat[0];s->player_reload=0;
        player_input.cycle_weapon=0; /* One posture request wins this sample. */
        if(rf_scene_combat_trace)printf("PLAYER_HOLSTER %u %u %u\n",frame,
            scene_player_weapon_holstered(),campaign_equipped_slot);
    }
    if(scene_player_weapon_holstered())scene_player_holster_release|=controls;
    scene_player_holster_mask();
}
#endif
