/* Installed shoulder_cannon is primary-only: no alt_fire flag, action or
 * alternate settings. Do not interpret the parser's alt defaults as an attack.
 * First-pass policy: immediate primary launch, held repetition at Fire Wait,
 * automatic reload on empty primary, explicit reload allowed. No charge-up or
 * homing invented. Launch backend must either publish one projectile with spawned1 and return
 * RF_OK, or report spawned0 for capacity rejection; it must not mutate this owner
 * or inventory. Parent routes the shot to existing real flight/blast services.
 * Tick once per60Hz simulation frame. Reset on new player/timeline; deselection,
 * death or inhibition cancels incomplete reload without moving ammunition. */
#include "rf/entity_assets.h"
#include <math.h>
typedef struct scene_fusion_input_state {
    uint32_t cooling,next_fire,reloading,reload_due;
} scene_fusion_input_state;
typedef struct scene_fusion_input_event {
    uint32_t shot,reload_started,reload_finished,rounds_loaded,dry;
} scene_fusion_input_event;
static void scene_fusion_input_reset(scene_fusion_input_state *state)
{scene_fusion_input_state empty={0};if(state)*state=empty;}

static int scene_fusion_input_tick(scene_fusion_input_state *state,
    rf_weapon_inventory *inventory,const rf_weapon_acquire_definition *supply,
    int32_t weapon,const rf_weapon_primary_definition *definition,uint32_t frame,
    uint32_t selected,uint32_t inhibited,uint32_t primary,uint32_t alternate,
    uint32_t reload,int (*launch)(void *,uint32_t *),void *context,scene_fusion_input_event *out)
{
    scene_fusion_input_event event={0};int status;uint32_t spawned=0;
    (void)alternate; /* No authored alternate behavior. */
    if(!state || !inventory || !supply || !definition || !out || !launch ||
       weapon<0 || weapon>=64 || supply->ammo_type<0 || supply->ammo_type>=32 ||
       supply->magazine!=1 || supply->capacity<0 ||
       !isfinite(definition->fire_seconds) || definition->fire_seconds<=0 || definition->fire_seconds>60 ||
       !isfinite(definition->reload_seconds) || definition->reload_seconds<=0 || definition->reload_seconds>60 ||
       inventory->loaded[weapon]<0 || inventory->loaded[weapon]>1 ||
       inventory->reserve[supply->ammo_type]<0 || inventory->reserve[supply->ammo_type]>supply->capacity)return RF_RANGE;
    if(!selected || inhibited || !inventory->owned[weapon]){
        scene_fusion_input_reset(state);*out=event;return RF_OK;
    }
    if(state->reloading){
        if((int32_t)(frame-state->reload_due)>=0){
            status=rf_weapon_reload_transfer(inventory,supply,weapon,&event.rounds_loaded);
            if(status)return status;
            state->reloading=0;event.reload_finished=1;
        }
        *out=event;return RF_OK;
    }
    if((reload || primary) && !inventory->loaded[weapon] && inventory->reserve[supply->ammo_type]>0){
        state->reloading=1;state->reload_due=frame+(uint32_t)ceilf(definition->reload_seconds*60.f);
        event.reload_started=1;
    }else if(primary && (!state->cooling || (int32_t)(frame-state->next_fire)>=0)){
        if(!inventory->loaded[weapon])event.dry=1;
        else{
            status=launch(context,&spawned);
            if(status==RF_NOT_FOUND){*out=event;return RF_OK;}
            if(status)return status;
            if(!spawned){*out=event;return RF_OK;}
            --inventory->loaded[weapon];event.shot=1;
            state->cooling=1;state->next_fire=frame+(uint32_t)ceilf(definition->fire_seconds*60.f);
        }
    }
    *out=event;return RF_OK;
}
