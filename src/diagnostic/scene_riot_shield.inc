/* Held Riot Shield interception, scene-owned and allocation free.
 * Binary:494e23..494e62 requires selected shield/live clutter handle and
 * dot(actor+60, travel vector)<0. 40a0b0 evaluates z+y+x.
 * 410270..4102c3 shares ordinary clutter damage arithmetic, then branches
 * to special shield destruction: player break presentation or NPC weapon
 * removal. We return broken so the scene owns that special lifecycle.
 * Port admission additionally requires a real nearer shield model contact;
 * this is deliberately NOT a guessed cone or front-hemisphere immunity.
 * No blast/fire environmental interception and no excess-damage spill policy.
 */
#include "rf/clutter_damage.h"
#include <math.h>

typedef struct scene_riot_shield_result {
    uint32_t intercepted,broken;
    rf_clutter_damage_state state;
} scene_riot_shield_result;

/* Borrow the resolved live shield owner and authored riot_shield definition.
 * contact must mean a shield-model intersection BEFORE the actor body, after
 * world obstruction; do not pass the actor hit itself as shield contact.
 * On broken, parent retires attached shield and removes its weapon selection.
 * Keep this owner across selection changes; reset only on new shield grant.
 * NULL/invalid math preserves result. Unselected/missing/broken/noncontact
 * shields pass damage through unchanged. No allocation or global state.
 */
static int scene_riot_shield_intercept(
    const rf_clutter_gameplay_definition *definition,
    const rf_clutter_damage_state *state,uint32_t selected,uint32_t live,
    uint32_t direct,uint32_t contact,const float forward[3],
    const float travel[3],float damage,int32_t damage_type,
    scene_riot_shield_result *result)
{
    scene_riot_shield_result value={0};rf_clutter_damage_result damage_result;
    double dot;uint32_t i;int status;
    if(!definition || !state || !forward || !travel || !result ||
       !isfinite(state->health) || !isfinite(damage) || damage<0)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(forward[i]) || !isfinite(travel[i]))return RF_RANGE;
    value.state=*state;
    dot=(double)forward[2]*travel[2]+(double)forward[1]*travel[1]+(double)forward[0]*travel[0];
    if(!selected || !live || !direct || !contact || state->health<=0 ||
       !(dot<0) || damage<.001f){*result=value;return RF_OK;}
    /* Only the common410270 arithmetic is reused; never ordinary prop
     * destruction/admission, which excludes the special shield owner. */
    status=rf_clutter_damage_apply(definition,state,damage,damage_type,&damage_result);
    if(status)return status;
    value.state=damage_result.state;value.state.object_flags|=0x200000u;
    value.intercepted=1;value.broken=damage_result.health_break_due;
    *result=value;return RF_OK;
}
