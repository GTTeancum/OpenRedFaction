#ifndef RF_SCENE_DRILLER_CHECKPOINT_ADAPTER_INC
#define RF_SCENE_DRILLER_CHECKPOINT_ADAPTER_INC
#include "rf/vehicle_checkpoint.h"
#include "rf/vehicle_rigid.h"
#include "rf/physics.h"
#include <math.h>
#include <string.h>
/* Include after actual scene_driller_runtime definitions. Helpers stage only:
 * parent must validate restored terrain/world, full host sphere placement,
 * player seat/exit placement and fresh registry identity before any commit.
 * Rebind semantic occupancy through normal possession; never copy session or
 * registrations. No disk, UI, sidecars, or mutation of live owners here. */
static int scene_driller_checkpoint_inertia(const float inverse[9],float tensor[9])
{
    uint32_t i,j;double det,minor;int status;
    for(i=0;i<9;i++)if(!isfinite(inverse[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=i+1;j<3;j++)if(fabs((double)inverse[3*i+j]-inverse[3*j+i])>
        1e-5*(fabs(inverse[3*i+j])+fabs(inverse[3*j+i])+1e-8))return RF_FORMAT;
    minor=(double)inverse[0]*inverse[4]-(double)inverse[1]*inverse[3];
    det=(double)inverse[0]*((double)inverse[4]*inverse[8]-(double)inverse[5]*inverse[7])-
        (double)inverse[1]*((double)inverse[3]*inverse[8]-(double)inverse[5]*inverse[6])+
        (double)inverse[2]*((double)inverse[3]*inverse[7]-(double)inverse[4]*inverse[6]);
    /* tensor_inverse intentionally copies a singular input; a save adapter
     * must reject it instead of silently inventing zero angular momentum. */
    if(inverse[0]<=0 || minor<=0 || !isfinite(det) || det<=0)return RF_FORMAT;
    status=rf_physics_tensor_inverse(inverse,tensor);return status;
}
static int scene_driller_checkpoint_rotate_tensor(const float basis[9],const float tensor[9],const float input[3],float output[3])
{
    float local[3]={0},transformed[3]={0},world[3]={0};uint32_t i,j;
    for(i=0;i<9;i++)if(!isfinite(basis[i]) || !isfinite(tensor[i]))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(input[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=0;j<3;j++)local[i]+=basis[i*3+j]*input[j];
    for(i=0;i<3;i++)for(j=0;j<3;j++)transformed[i]+=tensor[i*3+j]*local[j];
    for(i=0;i<3;i++)for(j=0;j<3;j++)world[i]+=basis[j*3+i]*transformed[j];
    for(i=0;i<3;i++)if(!isfinite(world[i]))return RF_FORMAT;
    memcpy(output,world,sizeof(world));return RF_OK;
}
typedef struct scene_driller_checkpoint_candidate {
    rf_vehicle_rigid_state physics;
    float health,armor,drill_spin;
    uint32_t alive,player_occupied,accepted_drill_cuts;
} scene_driller_checkpoint_candidate;
/* Pure math stage: live metadata/inertia remain current, saved orientation
 * determines angular-momentum reconstruction. Clears unsaved force transients. */
static int scene_driller_checkpoint_stage_rigid(const rf_vehicle_rigid_state *current,
    const rf_vehicle_checkpoint *record,scene_driller_checkpoint_candidate *out)
{
    scene_driller_checkpoint_candidate value={0};float tensor[9];int status;
    if(!current || !record || !out)return RF_RANGE;
    status=rf_vehicle_checkpoint_validate(record);if(status)return status;
    status=scene_driller_checkpoint_inertia(current->inverse_inertia,tensor);if(status)return status;
    memcpy(value.physics.inverse_inertia,current->inverse_inertia,sizeof(value.physics.inverse_inertia));
    memcpy(value.physics.position,record->position,12);memcpy(value.physics.orientation,record->orientation,36);
    memcpy(value.physics.velocity,record->velocity,12);
    status=scene_driller_checkpoint_rotate_tensor(record->orientation,tensor,record->angular_velocity,value.physics.momentum);if(status)return status;
    value.health=record->health;value.armor=record->armor;value.alive=record->alive;
    value.player_occupied=record->player_occupied;value.accepted_drill_cuts=record->accepted_drill_cuts;value.drill_spin=record->drill_spin;
    *out=value;return RF_OK;
}
#ifndef RF_SCENE_DRILLER_CHECKPOINT_MATH_ONLY
static int scene_driller_checkpoint_capture(const scene_stream *s,rf_vehicle_checkpoint *out)
{
    const scene_driller_runtime *r;rf_vehicle_checkpoint value={0};float tensor[9];int status;
    if(!s || !out)return RF_RANGE;
    r=s->driller_runtime;
    if(!r || !scene_driller_damage_runtime_valid(&r->damage) || r->damage.entry!=&r->entry ||
       r->entry.physics!=&r->physics)return RF_NOT_FOUND;
    /* RFVC has no persistent burn/effect timers. Refuse rather than discard a
     * continuing damage source. Blocked wreck ejection itself is supported. */
    if(r->damage.damage.state.effects.burn || r->damage.damage.destruction_pending)return RF_NOT_FOUND;
    memcpy(value.position,r->physics.state.position,12);memcpy(value.orientation,r->physics.state.orientation,36);
    memcpy(value.velocity,r->physics.state.velocity,12);
    status=scene_driller_checkpoint_inertia(r->physics.state.inverse_inertia,tensor);if(status)return status;
    status=scene_driller_checkpoint_rotate_tensor(value.orientation,r->physics.state.inverse_inertia,r->physics.state.momentum,value.angular_velocity);if(status)return status;
    value.health=r->damage.damage.state.effects.health;value.armor=r->damage.damage.state.effects.armor;
    value.alive=r->entry.host.alive;value.player_occupied=!!r->entry.session.active;
    value.accepted_drill_cuts=s->driller_weapon.accepted_cuts;value.drill_spin=s->driller_weapon.spin;
    status=rf_vehicle_checkpoint_validate(&value);if(status)return status;
    *out=value;return RF_OK;
}
static int scene_driller_checkpoint_stage(const scene_stream *s,const rf_vehicle_checkpoint *record,
    scene_driller_checkpoint_candidate *out)
{
    const scene_driller_runtime *r;
    if(!s || !record || !out)return RF_RANGE;
    r=s->driller_runtime;
    if(!r || !scene_driller_damage_runtime_valid(&r->damage) || r->damage.entry!=&r->entry ||
       r->entry.physics!=&r->physics)return RF_NOT_FOUND;
    return scene_driller_checkpoint_stage_rigid(&r->physics.state,record,out);
}
#endif
#endif
