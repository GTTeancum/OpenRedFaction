#ifndef SCENE_FIGHTER_CHECKPOINT_LIVE_INC
#define SCENE_FIGHTER_CHECKPOINT_LIVE_INC
#include "scene_fighter_checkpoint.inc"
/* Live ownership remains transient; wire data contains only portable state. */
static int scene_fighter_checkpoint_live_view(const scene_stream *s,scene_fighter_checkpoint_live *out)
{
    const scene_driller_runtime *r;scene_fighter_checkpoint_live v={0};
    if(!s || !out || !s->driller || strcmp(s->driller->model,"Fighter01.v3m"))return RF_RANGE;
    r=s->driller_runtime;
    if(!r || !scene_driller_damage_runtime_valid(&r->damage) || r->damage.entry!=&r->entry ||
       r->entry.physics!=&r->physics || r->entry.resource!=s->driller ||
       r->entry.player_view!=&campaign_player_view ||
       rf_entity_lookup(r->entry.entities,(int32_t)r->entry.player.handle)!=r->entry.player_view)return RF_NOT_FOUND;
    if(r->entry.session.active && (r->entry.session.host!=r->entry.host.handle ||
       r->entry.session.driver!=r->entry.player.handle || r->entry.host.driver!=r->entry.player.handle ||
       r->entry.player.host!=r->entry.host.handle || r->entry.occupant!=(int32_t)r->entry.player.handle ||
       r->entry.player_view->linked_handle!=(int32_t)r->entry.host.handle))return RF_NOT_FOUND;
    v.physics=&r->physics.state;v.weapon=&s->fighter_weapon;v.owner_valid=1;
    v.health=r->damage.damage.state.effects.health;v.armor=r->damage.damage.state.effects.armor;
    v.alive=r->entry.host.alive;v.occupied=!!r->entry.session.active;
    v.unsupported_effects=!!(r->damage.damage.state.effects.burn || r->damage.damage.destruction_pending);
    *out=v;return RF_OK;
}
static int scene_fighter_checkpoint_capture_live(const scene_stream *s,rf_fighter_checkpoint *out)
{
    scene_fighter_checkpoint_live live;scene_fighter_checkpoint_record captured;rf_fighter_checkpoint v={0};int status;
    if(!s || !out || !s->driller_runtime)return RF_RANGE;
    if(player_input.fire || player_input.alt_fire || player_input.use || s->driller_runtime->entry.held)return RF_NOT_FOUND;
    status=scene_fighter_checkpoint_live_view(s,&live);if(status)return status;
    status=scene_fighter_checkpoint_capture_state(&live,&captured);if(status)return status;
    v.vehicle=captured.vehicle;v.primary_reserve=captured.primary_reserve;v.rocket_reserve=captured.rocket_reserve;
    v.primary_cooldown=captured.primary_fire.cooldown;v.rocket_cooldown=captured.rocket_fire.cooldown;
    v.primary_shots=captured.primary_fire.shots;v.rocket_shots=captured.rocket_fire.shots;
    status=rf_fighter_checkpoint_validate(&v);if(status)return status;*out=v;return RF_OK;
}
static int scene_fighter_checkpoint_admit_parked(scene_stream *s)
{
    scene_fighter_checkpoint_live live;int status=scene_vehicle_checkpoint_parked_owner(s,0);if(status)return status;
    status=scene_fighter_checkpoint_live_view(s,&live);if(status)return status;
    return scene_fighter_checkpoint_quiet(&live);
}
static int scene_fighter_checkpoint_apply_live(scene_stream *s,const rf_fighter_checkpoint *v)
{
    scene_driller_checkpoint_candidate staged;int status;
    status=rf_fighter_checkpoint_validate(v);if(status)return status;
    status=scene_fighter_checkpoint_admit_parked(s);if(status)return status;
    status=scene_vehicle_combat_stage_validated(&s->driller_runtime->physics.state,&v->vehicle,&staged);if(status)return status;
    staged.player_occupied=0;
    scene_vehicle_checkpoint_publish_staged(s,&staged);
    memset(s->fighter_weapon.bullets,0,sizeof(s->fighter_weapon.bullets));
    memset(s->fighter_weapon.rockets,0,sizeof(s->fighter_weapon.rockets));
    memset(&s->fighter_weapon.primary_fire,0,sizeof(s->fighter_weapon.primary_fire));
    memset(&s->fighter_weapon.rocket_fire,0,sizeof(s->fighter_weapon.rocket_fire));
    s->fighter_weapon.primary_reserve=(int32_t)v->primary_reserve;s->fighter_weapon.rocket_reserve=(int32_t)v->rocket_reserve;
    s->fighter_weapon.primary_fire.cooldown=v->primary_cooldown;s->fighter_weapon.rocket_fire.cooldown=v->rocket_cooldown;
    s->fighter_weapon.primary_fire.shots=v->primary_shots;s->fighter_weapon.rocket_fire.shots=v->rocket_shots;
    return RF_OK;
}
#endif
