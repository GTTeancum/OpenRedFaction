#ifndef RF_SCENE_GRENADE_EMPTY_SELECTION_INCLUDED
#define RF_SCENE_GRENADE_EMPTY_SELECTION_INCLUDED

/* Original4c6627 resolves Grenade into872118;4a6f41..4a6f57 admits that
 * weapon even with Autoswitch Weapons disabled.4a2974..4a2981 calls the empty
 * consumer after projectile firing. Keep this live adapter grenade-only.
 * The port has no player+f41 preference owner/default yet: defer_flag=0 is
 * an explicit first-pass policy, preserving strict authored preference order.
 * Include after scene_grenade_gameplay.inc. See docs/GRENADE-EMPTY-SELECTION.md
 * for lifecycle and late-frame integration; no inventory/save ABI extension. */
static struct {
    int32_t preference[32];uint32_t ready,release_mask;
} scene_grenade_empty_selection;

static void scene_grenade_empty_input_reset(void)
{scene_grenade_empty_selection.release_mask=0;}

static void scene_grenade_empty_reset(void)
{
    memset(&scene_grenade_empty_selection,0,sizeof(scene_grenade_empty_selection));
    memset(scene_grenade_empty_selection.preference,0xff,sizeof(scene_grenade_empty_selection.preference));
}

static int scene_grenade_empty_load(rf_vpp *tables)
{
    int32_t preference[32];int status;
    status=rf_weapon_preference_load(tables,128*1024,&campaign_weapon_supply.names,preference);
    if(status)return status;
    memcpy(scene_grenade_empty_selection.preference,preference,sizeof(preference));
    scene_grenade_empty_selection.ready=1;scene_grenade_empty_input_reset();return RF_OK;
}

/* Call once per combat frame before any other helper masks handheld inputs.
 * Consume only controls still held across the automatic change. Automatic
 * weapons need this release latch as well as the trigger's semi-auto edge.
 * Vehicle input stays untouched; releases there may still clear the latch. */
static void scene_grenade_empty_input_tick(uint32_t on_foot)
{
    uint32_t held=(player_input.fire?1u:0u)|(player_input.alt_fire?2u:0u)|
        (player_input.reload?4u:0u);
    /* Special slots may never step the conventional trigger. Real release
     * must still retire its held edge after an automatic selection, or the
     * ordinary checkpoint guard would reject an otherwise settled owner. */
    if(!(held&3u))combat_trigger.held=0;
    scene_grenade_empty_selection.release_mask&=held;
    if(!on_foot)return;
    if(scene_grenade_empty_selection.release_mask&1u)player_input.fire=0;
    if(scene_grenade_empty_selection.release_mask&2u)player_input.alt_fire=0;
    if(scene_grenade_empty_selection.release_mask&4u)player_input.reload=0;
}

/* Shared automatic-selection input boundary. Both exhausted grenades and
 * real weapon acquisition from an explicitly unarmed state use this gate;
 * it never changes inventory or the independent grenade/projectile owner. */
static void scene_player_auto_select_inputs(void)
{
    uint32_t held=(player_input.fire?1u:0u)|(player_input.alt_fire?2u:0u)|
        (player_input.reload?4u:0u);
    combat_trigger.remaining=combat_trigger.delay=0;combat_trigger.held=!!(held&3u);
    rf_scene_combat[6]=0;campaign_last_alt=0;
    scene_grenade_empty_selection.release_mask|=held;
    player_input.fire=player_input.alt_fire=player_input.reload=0;
}

/* Authored weapon IDs and selectable/view slots are different namespaces.
 * Filtering the preference list never grants ownership or edits ammunition.
 * Remote's rank maps to its charge slot, never the shared-ID detonator slot.
 * A retained Machine Pistol mode is not silently changed: its actual mode
 * must be usable too, while the shared chooser still evaluates the base ID.
 * Original4a6e84..4a6ec6 tests that ranked base ID before4a4c91 remaps its
 * remembered mode; Special-only ammo does not pass the original scan either.
 * Neither Special nor Undercover has an authored rank to append or borrow. */
static int32_t scene_grenade_empty_slot(scene_stream *s,int32_t weapon)
{
    uint32_t slot,count=scene_weapon_slots();
    if(!s || weapon<0 || weapon>=64 ||
       (uint32_t)weapon>=campaign_weapon_supply.names.count)return -1;
    if(count>SCENE_WEAPON_SLOTS)count=SCENE_WEAPON_SLOTS;
    for(slot=0;slot<count;slot++){
        int32_t active,ammo;uint32_t view;
        const rf_weapon_acquire_definition *definition;
        if(slot==5 || slot==9 || slot==17 || !scene_weapon_available(slot) ||
           campaign_slot_weapon(slot)!=weapon)continue;
        view=slot==13 && campaign_machine_mode.special?17:slot;
        if(!s->player_weapon[view])continue;
        if((slot==4 && !scene_rocket_resources) ||
           (slot==8 && !scene_remote_resources) ||
           (slot==10 && !scene_flame_resources) ||
           (slot==11 && !rf_scene_player_shield_resources) ||
           (slot==12 && !scene_fusion_resources))continue;
        active=slot==13 && campaign_machine_mode.special?campaign_machine_special_id:weapon;
        if(active<0 || (uint32_t)active>=campaign_weapon_supply.names.count)continue;
        definition=campaign_weapon_supply.definitions+active;ammo=definition->ammo_type;
        if(slot!=11 && (ammo<0 || ammo>=32))continue;
        if(slot==13 && campaign_machine_mode.special && definition->capacity>0 &&
           (int64_t)campaign_player_inventory.loaded[active]+
               (ammo>=0?campaign_player_inventory.reserve[ammo]:0)<=0)continue;
        return (int32_t)slot;
    }
    return -1;
}

/* released is an operation result from successful projectile publication,
 * never a diagnostic counter delta. The caller runs this after all special
 * weapon/world ticks using the old selection, then ends weapon input only
 * when changed is set. Failed/full-pool throws and idle empty selections do
 * not enter this consumer; a remaining round keeps the current grenade. */
static int scene_grenade_empty_after_release(scene_stream *s,uint32_t frame,
    uint32_t released,uint32_t on_foot,uint32_t *changed)
{
    rf_weapon_supply supply[64];uint32_t flags_264[64];int32_t preference[32];
    rf_weapon_empty_input input={0};rf_weapon_empty_action action;
    uint32_t i;int32_t slot;int status;
    if(!s || !changed || released>1 || on_foot>1)return RF_RANGE;
    *changed=0;
    if(!released || !on_foot || campaign_equipped_slot!=5 || campaign_explicit_unarmed ||
       campaign_player_damage.state.effects.health<=0)return RF_OK;
    if(!scene_grenade_empty_selection.ready)return RF_NOT_FOUND;
    if(campaign_grenade_id<0 || campaign_grenade_id>=64 ||
       campaign_weapon_supply.names.count>64 || campaign_weapon_reset.names.count>64 ||
       (uint32_t)campaign_grenade_id>=campaign_weapon_supply.names.count)return RF_FORMAT;
    if(!campaign_player_inventory.owned[campaign_grenade_id])return RF_OK;
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
        preference[i]=scene_grenade_empty_slot(s,weapon)>=0?weapon:-1;
    }
    input.current=input.always_weapon=campaign_grenade_id;
    input.block_weapon=input.excluded_weapon=input.paired_first=input.paired_second=-1;
    /* automatic_enabled remains0: only the proven always-weapon exception.
     * No passenger/linked actor and no invented paired-mode firing request. */
    input.defer_flag=0;
    status=rf_weapon_decide_empty(&campaign_player_inventory,NULL,supply,flags_264,
        campaign_weapon_supply.names.count,preference,&input,&action);if(status)return status;
    if(action.kind!=RF_WEAPON_EMPTY_SELECT)return RF_OK;
    slot=scene_grenade_empty_slot(s,action.weapon);if(slot<0)return RF_FORMAT;
    campaign_select_primary((uint32_t)slot);
    scene_player_auto_select_inputs();
    /* Independent grenade cooldown and launched projectiles remain owned. */
    s->player_pose_frame=0;s->player_slot=UINT32_MAX;
    s->player_shots=rf_scene_combat[0];s->player_reload=0;
    campaign_ammo_publish();rf_scene_weapon_selection[0]=(uint32_t)slot;
    ++rf_scene_weapon_selection[1];*changed=1;
    if(rf_scene_combat_trace)printf("GRENADE_EMPTY_SELECT %u %d %d\n",frame,action.weapon,slot);
    return RF_OK;
}
#endif
