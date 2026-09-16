#include "rf/clutter_damage.h"
#include <math.h>
#include <float.h>
static int valid(const rf_clutter_gameplay_definition *d,
    const rf_clutter_damage_state *s,float damage,int32_t type,
    const rf_clutter_damage_result *out)
{
    if(!d || !s || !out || !isfinite(s->health) || !isfinite(damage) ||
       type< -1 || type>10 || (type>=0 && !isfinite(d->damage_factors[type])))return RF_RANGE;
    return RF_OK;
}
static void describe(rf_clutter_damage_result *r)
{
    r->health_break_due=r->state.health<=0;
    r->retirement_marked=(r->state.object_flags&2)!=0;
}
int rf_clutter_damage_apply(const rf_clutter_gameplay_definition *d,
    const rf_clutter_damage_state *s,float damage,int32_t type,
    rf_clutter_damage_result *out)
{
    rf_clutter_damage_result r;double effective,health;int status=valid(d,s,damage,type,out);
    if(status)return status;
    effective=(double)damage*(type<0?1.0:(double)d->damage_factors[type]);
    health=(double)s->health-effective;
    if(!isfinite(health) || health>FLT_MAX || health< -FLT_MAX)return RF_RANGE;
    r.state=*s;r.state.health=(float)health;r.admitted=1;
    /*41029c stores float but41029f compares the still-live x87 intermediate. */
    if(health<=0)r.state.killing_type=type;
    describe(&r);*out=r;return RF_OK;
}
int rf_clutter_damage_receive(const rf_clutter_gameplay_definition *d,
    const rf_clutter_damage_state *s,float damage,int32_t type,
    uint32_t override_protection,rf_clutter_damage_result *out)
{
    rf_clutter_damage_result r;int status=valid(d,s,damage,type,out);
    if(status)return status;
    r.state=*s;r.admitted=0;
    if(damage>=.001f){
        r.state.object_flags|=0x200000u;
        if(override_protection || !(r.state.object_flags&4u)){
            status=rf_clutter_damage_apply(d,&r.state,damage,type,&r);
            if(status)return status;
        }
    }
    describe(&r);*out=r;return RF_OK;
}
