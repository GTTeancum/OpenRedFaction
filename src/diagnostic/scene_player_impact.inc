/* Live physics contact aggregation, not position/fall-distance inference.
 * Parent begins each movement frame with enabled0 on spawn/respawn, teleport,
 * checkpoint relocation and diagnostic routes. Camera movement never submits
 * contact. Only actual sweep/contact-response hits call record; ordinary ground
 * support/snapping never does. Core49cd80 rejects low-speed standing/walking.
 * First-pass policy: one strongest eligible contact per frame, so multiple
 * spheres and response substeps do not multiply landing damage. Flush once
 * after final pose publication with current health/immunity/identity. */
#include "rf/entity.h"
#include <string.h>
typedef struct scene_player_impact {
    rf_entity_impact_actor contact;
    float speed,amount;
    uint32_t frame,enabled,ready,consumed;
} scene_player_impact;
static void scene_player_impact_begin(scene_player_impact *state,uint32_t frame,uint32_t enabled)
{
    if(!state)return;
    memset(state,0,sizeof(*state));state->frame=frame;state->enabled=enabled!=0;
}
static int scene_player_impact_record(scene_player_impact *state,uint32_t frame,
    uint32_t physical_contact,const rf_entity_impact_actor *actor,float speed)
{
    float amount;uint32_t eligible,falling;int status;
    if(!state || !actor)return RF_RANGE;
    if(state->frame!=frame || !state->enabled || state->consumed || !physical_contact)return RF_OK;
    if(actor->health<=0)return RF_OK;
    falling=rf_entity_falling((int32_t)actor->movement_mode,actor->use_kind,actor->support_material);
    status=rf_entity_impact_damage(speed,falling,actor->contact_material,actor->use_kind==1,
        actor->object_flags,&amount,&eligible);if(status)return status;
    if(eligible && (!state->ready || amount>state->amount)){
        state->contact=*actor;state->speed=speed;state->amount=amount;state->ready=1;
    }
    return RF_OK;
}
static int scene_player_impact_flush(scene_player_impact *state,uint32_t frame,
    const rf_entity_impact_actor *current,const rf_entity_impact_backend *backend)
{
    rf_entity_impact_actor actor;
    if(!state || !current || !backend)return RF_RANGE;
    if(state->frame!=frame || !state->enabled || state->consumed)return RF_OK;
    /* Consume before callbacks: downstream failure may follow health mutation,
     * so a caller retry must not apply the same landing twice. */
    state->consumed=1;
    if(!state->ready || current->health<=0)return RF_OK;
    if(current->handle!=state->contact.handle)return RF_NOT_FOUND;
    actor=*current;
    /* Preserve impact-time movement/contact inputs after landing changes mode,
     * while using current owner, position, health and immunity for dispatch. */
    actor.movement_mode=state->contact.movement_mode;
    actor.use_kind=state->contact.use_kind;
    actor.contact_material=state->contact.contact_material;
    actor.support_material=state->contact.support_material;
    return rf_entity_impact_process_sp(&actor,state->speed,backend);
}
