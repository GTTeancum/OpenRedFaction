#ifndef SCENE_CLUTTER_DAMAGE_LIVE_INC
#define SCENE_CLUTTER_DAMAGE_LIVE_INC
#include "rf/clutter.h"
#include "rf/clutter_damage.h"
#include <math.h>
#include <string.h>

/* One compact profile per authored CLASS, not model filename: classes sharing
 * a mesh can have different health/protection/factors. No retained resources. */
typedef struct scene_clutter_damage_profile {
    float life,damage_factors[11];
    uint32_t protected_object,ordinary;
    float break_radius,break_offset[3],break_damage;uint32_t break_yellboom;
} scene_clutter_damage_profile;
typedef struct scene_clutter_damage_binding {
    uint32_t handle,initialized,break_pending;
    int32_t killing_type,class_index;
} scene_clutter_damage_binding;
typedef struct scene_clutter_damage_live_result {
    uint32_t accepted,applied,retired;
    float health_before,health_after;
    int32_t killing_type;
} scene_clutter_damage_live_result;
static int scene_clutter_damage_profile_read(const void *text,uint32_t bytes,
    const char *name,scene_clutter_damage_profile *out)
{
    rf_clutter_gameplay_definition d;scene_clutter_damage_profile p={0};uint32_t i;int status;
    if(!out)return RF_RANGE;
    status=rf_clutter_gameplay_read(text,bytes,name,&d);if(status)return status;
    p.break_damage=d.explosion_damage; /* Multiplier until vclip resolution. */
    p.break_radius=d.explosion_radius;memcpy(p.break_offset,d.explosion_offset,12);p.break_yellboom=!strcmp(d.explosion,"yellboom");
    p.life=d.life;p.protected_object=d.life<0;p.ordinary=1;
    {const char *shield="riot_shield";const char *a=d.name;
        for(i=0;a[i] && shield[i];++i){unsigned c=(unsigned char)a[i];if(c>='A' && c<='Z')c+=32;if(c!=(unsigned char)shield[i])break;}
        if(!a[i] && !shield[i])p.ordinary=0;
    }
    memcpy(p.damage_factors,d.damage_factors,sizeof(p.damage_factors));*out=p;return RF_OK;
}
static int scene_clutter_damage_identity(const rf_object_registry *registry,
    const rf_clutter_base_owner *owner)
{
    return registry && owner && rf_object_registry_lookup(registry,owner->state.handle)==(const void *)owner;
}
/* Creation hook only. A repeated bind is idempotent; never reset damaged or
 * retired owners. Zero the binding only when its owner has been destroyed. */
static int scene_clutter_damage_bind(const rf_object_registry *registry,
    rf_clutter_base_owner *owner,const scene_clutter_damage_profile *profile,
    scene_clutter_damage_binding *binding)
{
    scene_clutter_damage_binding b={0};uint32_t i;
    if(!profile || !binding || !scene_clutter_damage_identity(registry,owner))return RF_RANGE;
    if(binding->initialized)return binding->handle==owner->state.handle && binding->class_index==owner->state.class_index?RF_OK:RF_RANGE;
    if(!isfinite(profile->life) || profile->ordinary>1 || profile->protected_object!=(uint32_t)(profile->life<0))return RF_RANGE;
    for(i=0;i<11;++i)if(!isfinite(profile->damage_factors[i]))return RF_RANGE;
    if(owner->state.flags&2)return RF_RANGE;
    b.handle=owner->state.handle;b.initialized=1;b.killing_type=-1;b.class_index=owner->state.class_index;
    if(profile->ordinary){
        owner->state.health=profile->life<0?100:profile->life;owner->state.armor=0;
        owner->state.flags=(owner->state.flags&~4u)|(profile->protected_object?4u:0u);
    }
    *binding=b;return RF_OK;
}
/* First-pass retirement hides/removes collision through bit2, but leaves the
 * registered allocation alive for hit events/deferred close. No fake explosion,
 * debris, corpse, respawn or source-attribution behavior is introduced here. */
static int scene_clutter_damage_receive_live(const rf_object_registry *registry,
    rf_clutter_base_owner *owner,const scene_clutter_damage_profile *profile,
    scene_clutter_damage_binding *binding,float damage,int32_t type,
    uint32_t override_protection,scene_clutter_damage_live_result *out)
{
    rf_clutter_gameplay_definition d={0};rf_clutter_damage_state s;rf_clutter_damage_result r;
    scene_clutter_damage_live_result value={0};int status;
    if(!out || !profile || !binding || !binding->initialized || !scene_clutter_damage_identity(registry,owner) ||
       binding->handle!=owner->state.handle || binding->class_index!=owner->state.class_index ||
       !isfinite(damage) || type< -1 || type>10)return RF_RANGE;
    value.health_before=value.health_after=owner->state.health;value.killing_type=binding->killing_type;
    if(!profile->ordinary || (owner->state.flags&(2u|0x4000u))){*out=value;return RF_OK;}
    memcpy(d.damage_factors,profile->damage_factors,sizeof(d.damage_factors));
    s.health=owner->state.health;s.object_flags=owner->state.flags;s.killing_type=binding->killing_type;
    status=rf_clutter_damage_receive(&d,&s,damage,type,override_protection,&r);if(status)return status;
    value.accepted=damage>=.001f;value.applied=r.admitted;
    value.retired=r.admitted && r.health_break_due;
    if(value.retired){r.state.object_flags|=2u;binding->break_pending=1;}
    owner->state.health=r.state.health;owner->state.flags=r.state.object_flags;binding->killing_type=r.state.killing_type;
    value.health_after=r.state.health;value.killing_type=r.state.killing_type;*out=value;return RF_OK;
}
#endif
