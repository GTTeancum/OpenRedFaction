#ifndef RF_SCENE_SCOPE_OWNER_INCLUDED
#define RF_SCENE_SCOPE_OWNER_INCLUDED

/* Original4a4d96 ->4aa0b0 ->4ad8a0 retires local zoom on an admitted
 * selection. The port's two-state scope is unchanged; this transient owner
 * prevents Sniper(slot6) and Precision Rifle(slot15) sharing active zoom.
 * Include after campaign_selected_weapon and its identity dependencies. */
typedef struct scene_scope_owner_state {
    uint32_t source,slot;
    int32_t weapon;
} scene_scope_owner_state;
static scene_scope_owner_state scene_scope_owner;

static void scene_scope_owner_clear(void)
{
    scene_scope.active=0;scene_scope_look=1;
    /* End-frame successful loads can run after cinematic FOV publication.
     * Retire ordinary zoom without taking that independent camera owner. */
    if(!campaign_cutscene_runtime.active)rf_scene_scope_projection=1;
}

/* New scene starts with held=0, retaining the existing initial input edge.
 * A successfully published load instead consumes its current alternate;
 * no provisional selector or fallible restore path may call this. */
static void scene_scope_owner_reset(uint32_t alternate)
{
    scene_scope_owner_clear();scene_scope.held=!!alternate;
    scene_scope_owner=(scene_scope_owner_state){0,UINT32_MAX,-1};
}

/* Requalify the complete generation-bearing player handle and actual weapon,
 * not merely the shared "is scoped" boolean. Failure never creates authority.
 * Call before the sole alternate-input step, then after successful combat and
 * deferred selection. The latter only retires ownership, never steps input.
 * Preserve held here: an already-held alternate cannot retoggle, while a
 * genuinely fresh simultaneous selection/alternate edge keeps port behavior.
 * Same qualified owner is a no-op, matching ordinary nonforced selection;
 * retail forced same-weapon resets are outside this ownership-only adapter. */
static uint32_t scene_scope_owner_reconcile(const scene_stream *s)
{
    scene_scope_owner_state next={0,UINT32_MAX,-1};
    uint32_t source=campaign_player_object.handle,slot=campaign_equipped_slot;
    if(s && (slot==6 || slot==15) && !campaign_explicit_unarmed &&
       !scene_driller_active(s) && !scene_turret_player_active() &&
       source && source!=UINT32_MAX &&
       campaign_player_object.view==&campaign_player_view &&
       (uint32_t)campaign_player_view.handle==source && campaign_player_view.type==0 &&
       campaign_player_damage.state.effects.handle==source &&
       rf_object_registry_lookup(&campaign_registry,source)==&campaign_player_object &&
       rf_entity_lookup(&campaign_entities,(int32_t)source)==&campaign_player_view &&
       scene_actor_body.allocated_bytes &&
       !((campaign_player_view.flags_7c|campaign_player_damage.object_flags)&(2u|0x4000u)) &&
       isfinite(campaign_player_damage.state.effects.health) &&
       campaign_player_damage.state.effects.health>0){
        int32_t weapon=campaign_selected_weapon();
        if(weapon>=0 && weapon<64 && campaign_weapon_supply.names.count<=64 &&
           (uint32_t)weapon<campaign_weapon_supply.names.count &&
           campaign_player_inventory.owned[weapon])
            next=(scene_scope_owner_state){source,slot,weapon};
    }
    if(!next.source || next.source!=scene_scope_owner.source ||
       next.slot!=scene_scope_owner.slot || next.weapon!=scene_scope_owner.weapon)
        scene_scope_owner_clear();
    scene_scope_owner=next;return next.source!=0;
}
#endif
