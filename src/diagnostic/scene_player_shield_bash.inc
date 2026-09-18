/* Allocation-free shield bash input timing. Caller resolves live melee body
 * contact/cover only on impact, and starts authored primary animation on attack.
 * Installed weapons.tbl riot shield: Damage10, Fire Wait.5, AI attack range2,
 * Impact Delay.2. The last field is not exposed by the shared primary parser;
 * supply its authored value explicitly. No ammunition is read or modified.
 * First-pass policy: held input repeats at authored fire wait; release finishes
 * an accepted swing; death/unequip cancels it. No catch-up attacks after gaps.
 * Call once per simulation frame at60Hz; reset on a new player/timeline. */
#include "rf/entity_assets.h"
#include <math.h>
typedef struct scene_player_shield_bash {
    uint32_t pending,cooling,impact_due,next_attack;
} scene_player_shield_bash;
typedef struct scene_player_shield_bash_event {
    uint32_t attack,impact;
    float damage,range;int32_t damage_type;
} scene_player_shield_bash_event;

static void scene_player_shield_bash_reset(scene_player_shield_bash *state)
{
    scene_player_shield_bash empty={0};if(state)*state=empty;
}

static int scene_player_shield_bash_tick(scene_player_shield_bash *state,
    const rf_weapon_primary_definition *definition,float impact_seconds,
    uint32_t frame,uint32_t equipped,uint32_t alive,uint32_t held,
    scene_player_shield_bash_event *out)
{
    scene_player_shield_bash_event event={0};
    uint32_t wait,delay;
    if(!state || !definition || !out ||
       !isfinite(definition->fire_seconds) || definition->fire_seconds<=0 ||
       definition->fire_seconds>3600 || !isfinite(impact_seconds) ||
       impact_seconds<0 || impact_seconds>definition->fire_seconds ||
       !isfinite(definition->damage) || definition->damage<=0 ||
       !isfinite(definition->ai_attack_range) || definition->ai_attack_range<=0)
        return RF_RANGE;
    if(!equipped || !alive){scene_player_shield_bash_reset(state);*out=event;return RF_OK;}
    wait=(uint32_t)ceilf(definition->fire_seconds*60.f);
    delay=(uint32_t)ceilf(impact_seconds*60.f);
    if(state->pending && (int32_t)(frame-state->impact_due)>=0){
        state->pending=0;event.impact=1;
    }
    if(!state->pending && held &&
       (!state->cooling || (int32_t)(frame-state->next_attack)>=0)){
        event.attack=1;state->cooling=1;state->next_attack=frame+wait;
        state->impact_due=frame+delay;state->pending=delay!=0;
        if(!delay)event.impact=1;
    }
    if(event.impact){event.damage=definition->damage;
        event.range=definition->ai_attack_range;event.damage_type=definition->damage_kind;}
    *out=event;return RF_OK;
}
