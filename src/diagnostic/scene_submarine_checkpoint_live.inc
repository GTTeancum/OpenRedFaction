#ifndef SCENE_SUBMARINE_CHECKPOINT_LIVE_INC
#define SCENE_SUBMARINE_CHECKPOINT_LIVE_INC
#include "scene_submarine_checkpoint.inc"
/* Live RFVC4 adapter. Registry handles, pending torpedoes and held controls
 * stay process-local; only the typed submarine state reaches the disk slot. */
static int scene_submarine_checkpoint_live_view(const scene_stream *s,scene_submarine_checkpoint_live *out)
{
    const scene_driller_runtime *r;scene_submarine_checkpoint_live v={0};
    if(!s||!out||!s->driller||strcmp(s->driller->model,"Sub_Mini01.v3m"))return RF_RANGE;
    r=s->driller_runtime;
    if(!r||!scene_driller_damage_runtime_valid(&r->damage)||r->damage.entry!=&r->entry||
       r->entry.physics!=&r->physics||r->entry.resource!=s->driller||
       r->entry.player_view!=&campaign_player_view||
       rf_entity_lookup(r->entry.entities,(int32_t)r->entry.player.handle)!=r->entry.player_view)return RF_NOT_FOUND;
    if(r->entry.session.active&&
       (r->entry.session.host!=r->entry.host.handle||r->entry.session.driver!=r->entry.player.handle||
        r->entry.host.driver!=r->entry.player.handle||r->entry.player.host!=r->entry.host.handle||
        r->entry.occupant!=(int32_t)r->entry.player.handle||
        r->entry.player_view->linked_handle!=(int32_t)r->entry.host.handle))return RF_NOT_FOUND;
    v.physics=&r->physics.state;v.owner_valid=1;
    v.health=r->damage.damage.state.effects.health;v.armor=r->damage.damage.state.effects.armor;
    v.alive=r->entry.host.alive;v.occupied=!!r->entry.session.active;
    v.torpedo_reserve=s->submarine_weapon.reserve;v.torpedo_cooldown=s->submarine_weapon.cooldown;
    v.active_torpedoes=scene_submarine_weapon_pending(s);
    v.unsupported_effects=!!(r->damage.damage.state.effects.burn||r->damage.damage.destruction_pending);
    *out=v;return RF_OK;
}
static int scene_submarine_checkpoint_capture_live(const scene_stream *s,rf_submarine_checkpoint *out)
{
    scene_submarine_checkpoint_live live;int status;
    if(!s||!out||!s->driller_runtime)return RF_RANGE;
    if(player_input.fire||player_input.alt_fire||player_input.use||s->driller_runtime->entry.held)return RF_NOT_FOUND;
    status=scene_submarine_checkpoint_live_view(s,&live);if(status)return status;
    return scene_submarine_checkpoint_capture_state(&live,out);
}
static int scene_submarine_checkpoint_admit_parked(scene_stream *s)
{
    scene_submarine_checkpoint_live live;int status=scene_vehicle_checkpoint_parked_owner(s,0);
    if(status)return status;
    status=scene_submarine_checkpoint_live_view(s,&live);if(status)return status;
    return live.active_torpedoes||live.unsupported_effects?RF_NOT_FOUND:RF_OK;
}
static int scene_submarine_checkpoint_apply_live(scene_stream *s,const rf_submarine_checkpoint *saved)
{
    scene_driller_checkpoint_candidate staged;int status;
    status=rf_submarine_checkpoint_validate(saved);if(status)return status;
    status=scene_submarine_checkpoint_admit_parked(s);if(status)return status;
    status=scene_vehicle_combat_stage_validated(&s->driller_runtime->physics.state,&saved->vehicle,&staged);
    if(status)return status;
    staged.player_occupied=0;
    scene_vehicle_checkpoint_publish_staged(s,&staged);
    memset(s->submarine_weapon.rounds,0,sizeof(s->submarine_weapon.rounds));
    s->submarine_weapon.reserve=(int32_t)saved->torpedo_reserve;
    s->submarine_weapon.cooldown=saved->torpedo_cooldown;
    return RF_OK;
}
#endif
