#ifndef RF_SCENE_UNARMED_PICKUP_SELECTION_INCLUDED
#define RF_SCENE_UNARMED_PICKUP_SELECTION_INCLUDED

/* Original45a9d1..45aa03 selects the accepted weapon when the primary is
 * negative, before the armed-player preference/Autoswitch branches. New
 * ownership and an accepted refill of an owned weapon both reach this join;
 * ammo-only45a500 does not. Include after campaign_ammo_publish, with
 * scene_player_auto_select_inputs forward-declared. No grant/save changes. */
static int32_t scene_unarmed_pickup_slot(scene_stream *s,int32_t weapon)
{
    uint32_t slot,count=scene_weapon_slots();
    if(!s || weapon<0 || weapon>=64 || campaign_weapon_supply.names.count>64 ||
       (uint32_t)weapon>=campaign_weapon_supply.names.count ||
       !campaign_player_inventory.owned[weapon])return -1;
    if(count>SCENE_WEAPON_SLOTS)count=SCENE_WEAPON_SLOTS;
    for(slot=0;slot<count;slot++){
        uint32_t view=slot;
        int32_t active=weapon;
        const rf_weapon_acquire_definition *definition;
        /* Remote's charge/detonator share ownership; only the charge is the
         * acquired primary. MP Special is a retained mode, never a new slot. */
        if(slot==9 || slot==17 || !scene_weapon_available(slot) ||
           campaign_slot_weapon(slot)!=weapon)continue;
        if(slot==13){
            if(campaign_machine_mode.special>1 || campaign_machine_mode.pending ||
               !s->player_weapon[13] || !s->player_weapon[17] ||
               !s->machine_custom[0] || !s->machine_custom[1])continue;
            if(campaign_machine_mode.special){view=17;active=campaign_machine_special_id;}
        }
        if(!s->player_weapon[view] || active<0 || active>=64 ||
           (uint32_t)active>=campaign_weapon_supply.names.count)continue;
        if((slot==4 && !scene_rocket_resources) ||
           (slot==5 && !scene_grenade_resources) ||
           (slot==8 && (!scene_remote_resources || !s->player_weapon[9])) ||
           (slot==10 && !scene_flame_resources) ||
           (slot==11 && !rf_scene_player_shield_resources) ||
           (slot==12 && !scene_fusion_resources) ||
           (slot==16 && (!s->undercover || s->undercover->mode.pending)))continue;
        definition=campaign_weapon_supply.definitions+active;
        if(slot!=11 && (definition->ammo_type<0 || definition->ammo_type>=32))continue;
        /* Ownership with zero granted rounds still selects. Never add an
         * ammo-availability policy or replace the preserved MP mode here. */
        return (int32_t)slot;
    }
    return -1;
}

/* Invoke only after successful real item/drop grant. The caller passes its
 * actual grant weapon ID, not a pickup-class number, view slot or guessed
 * first-owned fallback. A declined selection never rolls back inventory or
 * prevents normal pickup retirement/notice/audio. Returns1 only if changed. */
static uint32_t scene_unarmed_pickup_select(scene_stream *s,int32_t weapon,
    uint32_t gives_weapon,const rf_weapon_pickup_grant *grant)
{
    int32_t slot;
    if(!s || !grant || gives_weapon!=1 || (!grant->acquired && !grant->rounds) ||
       !campaign_explicit_unarmed || !(campaign_player_damage.state.effects.health>0) ||
       scene_driller_active(s) || scene_turret_player_active() ||
       campaign_player_form.active)return 0;
    slot=scene_unarmed_pickup_slot(s,weapon);if(slot<0)return 0;
    campaign_select_primary((uint32_t)slot);
    scene_player_auto_select_inputs();
    s->player_pose_frame=0;s->player_slot=UINT32_MAX;
    s->player_shots=rf_scene_combat[0];s->player_reload=0;
    campaign_ammo_publish();rf_scene_weapon_selection[0]=(uint32_t)slot;
    ++rf_scene_weapon_selection[1];
    if(rf_scene_combat_trace)printf("UNARMED_PICKUP_SELECT %u %d %d %u %u\n",
        combat_frame,weapon,slot,grant->acquired,grant->rounds);
    return 1;
}
#endif
