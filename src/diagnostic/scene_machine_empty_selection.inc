#ifndef RF_SCENE_MACHINE_EMPTY_SELECTION_INCLUDED
#define RF_SCENE_MACHINE_EMPTY_SELECTION_INCLUDED

/* RF.exe4a7016..7074 requests alternate custom mode, not a shot or instant
 * ranked selection. For MP,4a520c..5369 returns through42a6e0/42a7c0 before
 * ordinary firing. Admit only the accepted dry operation: original final-shot
 * callback follows owner+4b8 cooldown publication, while dry reset41ae70 reaches
 * empty41b02c before dry backoff. No persistent queue, new save owner or grant.
 * Include after scene_machine_pistol_input.inc; conventional consumer remains
 * separate. The caller passes the same actual position as the combat tick. */
static int scene_machine_empty_after_combat(scene_stream *s,uint32_t frame,const float position[3],
    const scene_weapon_empty_request *request,uint32_t *changed)
{
    rf_weapon_supply supply[64];uint32_t flags_264[64];int32_t preference[32];
    rf_weapon_empty_input input={0};rf_weapon_empty_action action;
    const rf_weapon_acquire_definition *current,*other;
    int32_t base=campaign_extra_ids[0],special=campaign_machine_special_id,ammo,other_weapon,other_ammo,slot;
    uint32_t i;int status;
    if(!s || !position || !request || !changed)return RF_RANGE;
    *changed=0;
    if(request->slot!=13 || campaign_equipped_slot!=13 ||
       (request->operation!=SCENE_WEAPON_EMPTY_SHOT && request->operation!=SCENE_WEAPON_EMPTY_DRY) ||
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
       (campaign_player_view.flags_810&(1u|0x800u)) || rf_scene_combat[6] ||
       campaign_machine_mode.special>1 || campaign_machine_mode.pending)return RF_OK;
    if(campaign_weapon_supply.names.count>64 || campaign_weapon_reset.names.count>64 ||
       base<0 || base>=64 || special<0 || special>=64 || base==special ||
       (uint32_t)base>=campaign_weapon_supply.names.count ||
       (uint32_t)special>=campaign_weapon_supply.names.count)return RF_FORMAT;
    if(request->weapon!=(campaign_machine_mode.special?special:base) ||
       !campaign_player_inventory.owned[base] || campaign_player_inventory.owned[special] ||
       scene_unarmed_pickup_slot(s,base)!=13)return RF_OK;
    /* The outgoing Special ID is deliberately unowned. Admit with owned base
     * and actual active ID; never feed Special to the ranked-base predicate. */
    current=campaign_weapon_supply.definitions+request->weapon;ammo=current->ammo_type;
    other_weapon=campaign_machine_mode.special?base:special;
    other=campaign_weapon_supply.definitions+other_weapon;other_ammo=other->ammo_type;
    if(current->capacity<=0 || current->magazine<=0 || ammo<0 || ammo>=32 ||
       other->capacity<=0 || other->magazine<=0 || other_ammo<0 || other_ammo>=32 ||
       ammo==other_ammo)return RF_FORMAT;
    if(campaign_player_inventory.loaded[request->weapon]!=0 || campaign_player_inventory.reserve[ammo]!=0 ||
       campaign_player_inventory.loaded[other_weapon]<0 || campaign_player_inventory.reserve[other_ammo]<0)return RF_OK;
    for(i=0;i<2;i++){
        const scene_weapon_custom_actions *custom=s->machine_custom[i];
        const rf_player_weapon *view=s->player_weapon[i?17:13];
        if(!custom || !view || custom->view!=view || custom->count!=1 || custom->active!=-1 ||
           !custom->payloads[0] || view->payloads[3] || view->clip_count!=custom->old_count ||
           !view->bone_count || !s->machine_transition_ticks[i] || s->machine_transition_ticks[i]>3600)return RF_OK;
    }
    if(!scene_grenade_empty_selection.ready)return RF_NOT_FOUND;
    memset(supply,0,sizeof(supply));memset(flags_264,0,sizeof(flags_264));
    for(i=0;i<campaign_weapon_supply.names.count;i++){
        int32_t descriptor=rf_weapon_name_find(&campaign_weapon_reset.names,
            campaign_weapon_supply.names.names[i]);
        if(descriptor<0)return RF_FORMAT;
        supply[i].ammo_type=campaign_weapon_supply.definitions[i].ammo_type;
        supply[i].capacity=campaign_weapon_supply.definitions[i].capacity;
        supply[i].flags_268=campaign_weapon_reset.definitions[descriptor].flags_268;
        flags_264[i]=campaign_weapon_reset.definitions[descriptor].flags_264;
    }
    /* Both authored modes have alt_custom_mode and no looping alternate.
     * Bit19 admits the custom branch; its separate continuous-PRIMARY test
     * is handled at accepted dry reset, not by the alternate-looping guard. */
    if(!(flags_264[base]&0x80000u) || !(flags_264[special]&0x80000u) ||
       (flags_264[base]&4u) || (flags_264[special]&4u))return RF_FORMAT;
    for(i=0;i<32;i++){
        int32_t weapon=scene_grenade_empty_selection.preference[i];
        preference[i]=scene_weapon_empty_rank_slot(s,weapon)>=0?weapon:-1;
    }
    input.current=request->weapon;
    input.always_weapon=input.block_weapon=input.excluded_weapon=-1;
    input.paired_first=base;input.paired_second=special;
    input.automatic_enabled=1;input.defer_flag=0;input.override_mode=0; /* Real SP policy. */
    status=rf_weapon_decide_empty(&campaign_player_inventory,NULL,supply,flags_264,
        campaign_weapon_supply.names.count,preference,&input,&action);if(status)return status;
    if(action.kind==RF_WEAPON_EMPTY_PAIR){
        scene_machine_pistol_mode_state next=campaign_machine_mode;
        scene_machine_pistol_mode_event event;
        /* A successful final shot is a decision boundary but cannot skip its
         * actual cooldown to start a pair action. No retained request is made;
         * the next real accepted dry trigger is independently qualified. */
        if(request->operation!=SCENE_WEAPON_EMPTY_DRY || player_input.reload ||
           combat_trigger.cooldown || combat_trigger.remaining || combat_trigger.delay)return RF_OK;
        status=scene_machine_pistol_mode_begin(&next,&campaign_player_inventory,base,special,
            s->machine_transition_ticks,frame,&event);
        if(!status)status=scene_machine_mode_apply(s,frame,position,&next,&event,1);
        rf_scene_machine_mode[7]=(uint32_t)status;if(status)return status;
        *changed=1;
        if(rf_scene_combat_trace)printf("MACHINE_EMPTY_PAIR %u %d %d %u\n",
            frame,request->weapon,other_weapon,campaign_machine_mode.due);
        return RF_OK;
    }
    if(action.kind!=RF_WEAPON_EMPTY_SELECT)return RF_OK;
    slot=scene_weapon_empty_rank_slot(s,action.weapon);
    if(slot<0)return RF_FORMAT;
    if(slot==13)return RF_OK;
    campaign_select_primary((uint32_t)slot);scene_player_auto_select_inputs();
    s->player_pose_frame=0;s->player_slot=UINT32_MAX;
    s->player_shots=rf_scene_combat[0];s->player_reload=0;
    campaign_ammo_publish();rf_scene_weapon_selection[0]=(uint32_t)slot;
    ++rf_scene_weapon_selection[1];*changed=1;
    if(rf_scene_combat_trace)printf("MACHINE_EMPTY_SELECT %u %d %d %d\n",
        frame,request->weapon,action.weapon,slot);
    return RF_OK;
}
#endif
