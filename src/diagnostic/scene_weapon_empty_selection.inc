#ifndef RF_SCENE_WEAPON_EMPTY_SELECTION_INCLUDED
#define RF_SCENE_WEAPON_EMPTY_SELECTION_INCLUDED

/* RF.exe factory initialization passes player+e4 to4afe50, which writes
 * subobject+e5c=1 and+e5d=0 at4afed2/4afed8: player+f40 Autoswitch is ON,
 * player+f41 explosive defer is OFF. Profile loading may override both; this
 * port has no profile owner, so the verified factory policy is explicit.
 * This conventional/Fusion-shot/Flame-fuel subset uses the existing authored
 * preference owner, actual inventory and admitted resources. Include after
 * scene_weapon_auto_reload.inc and the shared request declarations in scene.c.
 * No retained state, save field or ammo grant.
 * The frame-local note also captures MP; its distinct paired action consumer
 * lives in scene_machine_empty_selection.inc. See WEAPON-EMPTY-SELECTION.md. */
static uint32_t scene_weapon_empty_conventional_slot(uint32_t slot)
{
    switch(slot){
    case 0:case 1:case 3:case 4:case 6:case 7:
    case 14:case 15:case 16:return 1;
    default:return 0;
    }
}

/* Call only at an actual successful conventional/Fusion shot, Flame primary
 * fuel debit or an accepted conventional no-ammo trigger result. Capture
 * inside the operation, after cycle/pickup input; a pre-tick may name another
 * weapon. Fusion dry and Flame alternate/reload/dry are not accepted here. */
static void scene_weapon_empty_note(scene_weapon_empty_request *request,uint32_t operation)
{
    if(!request)return;
    if(operation==SCENE_WEAPON_EMPTY_FUEL_DEBIT){
        if(campaign_equipped_slot!=10 || campaign_selected_weapon()!=campaign_flame_id)return;
    }else if((operation!=SCENE_WEAPON_EMPTY_SHOT && operation!=SCENE_WEAPON_EMPTY_DRY) ||
       (!scene_weapon_empty_conventional_slot(campaign_equipped_slot) && campaign_equipped_slot!=13 &&
        !(campaign_equipped_slot==12 && operation==SCENE_WEAPON_EMPTY_SHOT)))return;
    request->source=campaign_player_object.handle;
    request->slot=campaign_equipped_slot;
    request->weapon=campaign_selected_weapon();
    request->operation=operation;
}

static int32_t scene_weapon_empty_rank_slot(scene_stream *s,int32_t weapon)
{
    int32_t slot=scene_unarmed_pickup_slot(s,weapon);
    const rf_weapon_acquire_definition *definition;
    int32_t active,ammo;
    if(slot<0)return -1;
    /* Unlike grenade depletion, a conventional replacement may itself be a
     * grenade. The shared pickup admission also requires Grenade resources,
     * both Remote views and both MP mode views/custom-transition resources. */
    active=slot==13 && campaign_machine_mode.special?campaign_machine_special_id:weapon;
    definition=campaign_weapon_supply.definitions+active;
    ammo=definition->ammo_type;
    /* Keep the completed MP mode. The shared chooser still tests the ranked
     * base ID's ownership/ammo; Special-only supply never borrows its rank.
     * As in grenade selection, additionally reject an unusable retained mode. */
    if(slot==13 && campaign_machine_mode.special && definition->capacity>0 &&
       (ammo<0 || ammo>=32 ||
        (int64_t)campaign_player_inventory.loaded[active]+
            campaign_player_inventory.reserve[ammo]<=0))return -1;
    return slot;
}

/* Run once only after a successful campaign_combat_tick, before later view
 * advancement. All outgoing contacts/audio/pellets use their captured weapon
 * before selection changes. A failed shot/world operation never reaches this
 * consumer. The frame-local request is not a counter delta or retained event. */
static int scene_weapon_empty_after_combat(scene_stream *s,uint32_t frame,const float position[3],
    const scene_weapon_empty_request *request,uint32_t *changed)
{
    rf_weapon_supply supply[64];uint32_t flags_264[64];int32_t preference[32];
    rf_weapon_empty_input input={0};rf_weapon_empty_action action;
    const rf_weapon_acquire_definition *current;
    uint32_t i;int32_t slot,ammo;int status;
    if(!s || !position || !request || !changed)return RF_RANGE;
    *changed=0;
    if(request->weapon<0 || request->weapon>=64 ||
       (request->operation==SCENE_WEAPON_EMPTY_FUEL_DEBIT?
        (request->slot!=10 || request->weapon!=campaign_flame_id):
        (!scene_weapon_empty_conventional_slot(request->slot) &&
         !(request->slot==12 && request->operation==SCENE_WEAPON_EMPTY_SHOT))) ||
       !request->source || request->source==UINT32_MAX ||
       request->source!=campaign_player_object.handle ||
       campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=request->source || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=request->source ||
       rf_object_registry_lookup(&campaign_registry,request->source)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)request->source)!=&campaign_player_view ||
       !scene_actor_body.allocated_bytes ||
       ((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) ||
       request->slot!=campaign_equipped_slot || request->weapon!=campaign_selected_weapon() ||
       campaign_explicit_unarmed || campaign_player_form.active ||
       campaign_cutscene_runtime.active || scene_driller_active(s) || scene_turret_player_active() ||
       !isfinite(campaign_player_damage.state.effects.health) ||
       !(campaign_player_damage.state.effects.health>0) || rf_scene_enemy_combat[6] ||
       (campaign_player_view.flags_810&(1u|0x800u)) || rf_scene_combat[6])return RF_OK;
    if(campaign_weapon_supply.names.count>64 || campaign_weapon_reset.names.count>64 ||
       (uint32_t)request->weapon>=campaign_weapon_supply.names.count)return RF_FORMAT;
    if(scene_weapon_empty_rank_slot(s,request->weapon)!=(int32_t)request->slot)return RF_OK;
    current=campaign_weapon_supply.definitions+request->weapon;ammo=current->ammo_type;
    if(current->capacity<=0 || current->magazine<=0 || ammo<0 || ammo>=32)return RF_OK;
    /* Exact zero declines malformed negative inventory instead of relying on
     * the original wrapping/nonpositive arithmetic. Real positive reserve
     * leaves reload to its existing owner, including autonomous reload. */
    if(campaign_player_inventory.loaded[request->weapon]!=0 ||
       campaign_player_inventory.reserve[ammo]!=0)return RF_OK;
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
    for(i=0;i<32;i++){
        int32_t weapon=scene_grenade_empty_selection.preference[i];
        preference[i]=scene_weapon_empty_rank_slot(s,weapon)>=0?weapon:-1;
    }
    input.current=request->weapon;
    input.always_weapon=input.block_weapon=input.excluded_weapon=-1;
    input.paired_first=input.paired_second=-1;
    input.automatic_enabled=1;input.defer_flag=0;
    /* The admitted current slots cannot be either Remote identity, Grenade
     * or either MP identity. Their original exceptions/paired requests are
     * not approximated with sentinel IDs or an invented mode selection. */
    status=rf_weapon_decide_empty(&campaign_player_inventory,NULL,supply,flags_264,
        campaign_weapon_supply.names.count,preference,&input,&action);if(status)return status;
    if(action.kind!=RF_WEAPON_EMPTY_SELECT)return RF_OK;
    slot=scene_weapon_empty_rank_slot(s,action.weapon);
    if(slot<0)return RF_FORMAT;
    if((uint32_t)slot==request->slot)return RF_OK;
    campaign_select_primary((uint32_t)slot);
    /* Apply ordinary Flame deselection now, before a same-frame RFAP save.
     * Use the completed combat position for release Foley; do not tick again
     * or reset fractional fuel, cadence, alt cooldown/held, flights or burns. */
    if(request->slot==10){
        scene_flame_input_stop(position);
        scene_flame_input.reload_left=scene_flame_input.reload_elapsed=scene_flame_input.drained=0;
        scene_flame_canister_pending=scene_flame_active=0;
    }
    /* RFAP permits live Fusion flights with another selected weapon, but
     * its input cooldown/reload owner must agree with selection immediately.
     * Retire only input after a real replacement; never reset the flights. */
    if(request->slot==12)scene_fusion_input_reset(&scene_fusion_input);
    scene_player_auto_select_inputs();
    s->player_pose_frame=0;s->player_slot=UINT32_MAX;
    s->player_shots=rf_scene_combat[0];s->player_reload=0;
    campaign_ammo_publish();rf_scene_weapon_selection[0]=(uint32_t)slot;
    ++rf_scene_weapon_selection[1];*changed=1;
    if(rf_scene_combat_trace)printf("WEAPON_EMPTY_SELECT %u %d %d %d\n",
        frame,request->weapon,action.weapon,slot);
    return RF_OK;
}
#endif
