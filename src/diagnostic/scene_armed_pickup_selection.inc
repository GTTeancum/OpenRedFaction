#ifndef RF_SCENE_ARMED_PICKUP_SELECTION_INCLUDED
#define RF_SCENE_ARMED_PICKUP_SELECTION_INCLUDED

/* RF.exe45aa27..45aa98: local armed player, Autoswitch==1, NEW ownership,
 * optional explosive defer, then acquired rank strictly less than actual
 * current-primary rank.4a23c0 returns the first authored position or -1.
 * Keep that signed comparison; do not normalize MP Special/Detonator to their
 * ranked inventory base. Include after scene_grenade_empty_selection.inc;
 * scene_armed_pickup_request and the entry point are declared before pickups.
 * Factory player+f40=1,+f41=0 is proven at4afed2/4afed8. No profile owner is
 * implied. This selection-only adapter never grants, refills or saves state. */
static int32_t scene_armed_pickup_rank(int32_t weapon)
{
    uint32_t i;
    for(i=0;i<32;i++)if(scene_grenade_empty_selection.preference[i]==weapon)return (int32_t)i;
    return -1;
}

/* The successful real grant supplies weapon/gives_weapon/acquired. Capture
 * request immediately before that grant, then consume after normal pickup
 * retirement/feedback and only if the unarmed consumer did not select.
 * Declining selection cannot undo or fail the accepted inventory operation. */
static uint32_t scene_armed_pickup_select(scene_stream *s,int32_t weapon,
    uint32_t gives_weapon,const rf_weapon_pickup_grant *grant,
    const scene_armed_pickup_request *request)
{
    const uint32_t automatic_enabled=1,defer_flag=0;
    int32_t current,base,slot,descriptor,ammo,rank,current_rank;
    uint32_t current_slot;
    if(!s || !grant || !request || gives_weapon!=1 || grant->acquired!=1 ||
       automatic_enabled!=1 || !scene_grenade_empty_selection.ready ||
       !request->source || request->source==UINT32_MAX ||
       request->source!=campaign_player_object.handle ||
       campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=request->source || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=request->source ||
       rf_object_registry_lookup(&campaign_registry,request->source)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)request->source)!=&campaign_player_view ||
       !scene_actor_body.allocated_bytes ||
       ((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) ||
       campaign_explicit_unarmed || campaign_player_form.active || campaign_cutscene_runtime.active ||
       scene_driller_active(s) || scene_turret_player_active() ||
       !isfinite(campaign_player_damage.state.effects.health) ||
       !(campaign_player_damage.state.effects.health>0) || rf_scene_enemy_combat[6] ||
       (campaign_player_view.flags_810&(1u|0x800u)) ||
       request->slot!=campaign_equipped_slot || request->slot>=scene_weapon_slots() ||
       request->slot>=SCENE_WEAPON_SLOTS || campaign_weapon_supply.names.count>64 ||
       campaign_weapon_reset.names.count>64)return 0;
    /* Slot9 intentionally shares Charge inventory in this port, but its
     * original current-primary identity is Detonator for preference lookup. */
    current=request->slot==9?campaign_detonator_id:campaign_selected_weapon();
    if(current!=request->weapon || current<0 || current>=64 ||
       (uint32_t)current>=campaign_weapon_supply.names.count || weapon<0 || weapon>=64 ||
       (uint32_t)weapon>=campaign_weapon_supply.names.count)return 0;
    base=campaign_slot_weapon(request->slot);
    current_slot=request->slot==9?8u:request->slot;
    if(scene_unarmed_pickup_slot(s,base)!=(int32_t)current_slot)return 0;
    slot=scene_unarmed_pickup_slot(s,weapon);if(slot<0)return 0;
    descriptor=rf_weapon_name_find(&campaign_weapon_reset.names,
        campaign_weapon_supply.names.names[weapon]);
    if(descriptor<0)return 0;
    /*4c9a70 reads descriptor+268 bit8. Unlike depletion's chooser, pickup
     * compares just this actual acquired weapon against the actual current. */
    if(defer_flag && (campaign_weapon_reset.definitions[descriptor].flags_268&0x100u))return 0;
    rank=scene_armed_pickup_rank(weapon);current_rank=scene_armed_pickup_rank(current);
    if(rank>=current_rank)return 0;
    /* There is no general positive-ammo admission in the pickup tail. Keep
     * empty ordinary new guns and the completed MP mode; only the downstream
     * Grenade/Remote dispatcher has these separate mapped-reserve branches. */
    if(slot==5 || slot==8){
        ammo=campaign_weapon_supply.definitions[weapon].ammo_type;
        if(ammo<0 || ammo>=32 || campaign_player_inventory.reserve[ammo]<0)return 0;
        if(slot==5 && campaign_player_inventory.reserve[ammo]<=0)return 0; /*4a4c34..90*/
        if(slot==8 && (campaign_player_inventory.reserve[ammo]==0 ||
            scene_remote_selection_has_charge())){ /*4a4abb..4b34*/
            if(campaign_detonator_id<0 || campaign_detonator_id>=64 ||
               (uint32_t)campaign_detonator_id>=campaign_weapon_supply.names.count ||
               !scene_weapon_available(9) || !s->player_weapon[9])return 0;
            slot=9;
        }
    }
    if((uint32_t)slot==request->slot)return 0;
    /*45aa93 requests force0: keep the grant, refuse only this selection. */
    if(campaign_reload_blocks_ordinary_selection())return 0;
    if(slot!=2)scene_player_riot_cancel();
    campaign_select_primary((uint32_t)slot);scene_player_auto_select_inputs();
    s->player_pose_frame=0;s->player_slot=UINT32_MAX;
    s->player_shots=rf_scene_combat[0];s->player_reload=0;
    campaign_ammo_publish();rf_scene_weapon_selection[0]=(uint32_t)slot;
    ++rf_scene_weapon_selection[1];
    if(rf_scene_combat_trace)printf("ARMED_PICKUP_SELECT %u %d %d %d %d %d\n",
        combat_frame,current,weapon,slot,current_rank,rank);
    return 1;
}
#endif
