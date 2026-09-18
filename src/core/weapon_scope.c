#include "rf/weapon_scope.h"
#include "rf/weapon.h"
#include <math.h>
int rf_weapon_scope_step(rf_weapon_scope *state,uint32_t alternate,uint32_t selected,
    uint32_t alive,float world_fov,float zoom_fov,rf_weapon_scope_result *out)
{
    rf_weapon_scope next;rf_weapon_scope_result result;
    if(!state || !out || state->active>1 || state->held>1 || alternate>1 ||
       selected>1 || alive>1 || !isfinite(world_fov) || !isfinite(zoom_fov) ||
       world_fov<1 || world_fov>179 || zoom_fov<1 || zoom_fov>world_fov)return RF_RANGE;
    next=*state;
    if(!selected || !alive)next.active=0;
    else if(alternate && !next.held)next.active=!next.active;
    next.held=alternate;result.active=next.active;result.changed=next.active!=state->active;
    result.horizontal_fov=next.active?zoom_fov:world_fov;
    result.projection_scale=next.active?(float)(tan(world_fov*0.008726646259971648)/tan(zoom_fov*0.008726646259971648)):1;
    result.look_scale=1/result.projection_scale;
    *state=next;*out=result;return RF_OK;
}
