#ifndef RF_SCENE_DRILLER_DAMAGE_INC
#define RF_SCENE_DRILLER_DAMAGE_INC
#include <math.h>
/* Include after scene_driller_resources.inc. Core owns actual entity damage;
 * parent owns identity registration, effects, occupant detach and destruction.
 * Callback errors preserve committed damage and leave destruction pending. */
typedef struct scene_driller_damage {
    rf_entity_damage_state state;float factors[11];uint32_t object_flags,class_flags;
    uint32_t destroyed,destruction_pending;
} scene_driller_damage;
typedef int (*scene_driller_destroyed)(void *,uint32_t host,uint32_t responsible);
static int scene_vehicle_damage_open(const char *class_name,scene_driller_damage *owner,rf_vpp *tables,
    const scene_driller_resources *resources,uint32_t handle,uint32_t object_flags,
    uint32_t affiliation,uint32_t scratch_budget)
{
    scene_driller_damage value={0};rf_entity_creation_vitals_state vitals={0};rf_vpp_entry entry;void *text;int status;
    if(!class_name || !owner || !tables || !resources || handle==UINT32_MAX)return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)return status;
    if(entry.size>scratch_budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_entity_damage_factors_read(text,entry.size,class_name,value.factors);
    free(text);if(status)return status;
    rf_entity_creation_vitals(&vitals,&resources->vitals,0);
    value.state.effects.health=vitals.health;value.state.effects.armor=vitals.armor;
    value.state.effects.class_health=resources->vitals.health;value.state.effects.class_armor=resources->vitals.armor;
    value.state.effects.class_flags_728=resources->physics.authored.flags2;
    value.state.effects.handle=handle;value.state.effects.affiliation=affiliation;
    value.state.effects.voice=UINT32_MAX;value.state.responsible_handle=UINT32_MAX;
    value.object_flags=object_flags|vitals.object_flags;value.class_flags=resources->physics.authored.flags;
    *owner=value;return RF_OK;
}
static int scene_driller_damage_open(scene_driller_damage *owner,rf_vpp *tables,const scene_driller_resources *resources,
    uint32_t handle,uint32_t flags,uint32_t affiliation,uint32_t budget)
{return scene_vehicle_damage_open("Driller01",owner,tables,resources,handle,flags,affiliation,budget);}

typedef struct scene_driller_damage_context {
    scene_driller_damage *owner;rf_damage_object object;const rf_damage_effect_backend *effects;
    uint32_t clock,linked_player;int status;
} scene_driller_damage_context;
static rf_damage_object *scene_driller_damage_lookup(void *context,uint32_t handle)
{
    scene_driller_damage_context *c=context;
    return c->owner->state.effects.handle==handle?&c->object:NULL;
}
static uint32_t scene_driller_damage_predicate(void *context,uint32_t stage,uint32_t handle,const rf_damage_object *object)
{
    scene_driller_damage_context *c=context;(void)handle;(void)object;
    if(stage==0)return 1;
    if(stage==1)return rf_entity_armor_immunity(c->owner->state.effects.armor,c->owner->class_flags,c->owner->state.effects.flags_814);
    return c->linked_player;
}
static float scene_driller_damage_effect(void *context,rf_damage_object *object,float amount,uint32_t source,int32_t kind,uint32_t extra)
{
    scene_driller_damage_context *c=context;float result=0;
    c->owner->object_flags=object->flags;
    c->status=rf_entity_damage_sp(&c->owner->state,amount,kind,source,(int32_t)extra,
        kind<0?1:c->owner->factors[kind],c->clock,c->effects,&result);
    object->health=c->owner->state.effects.health;return result;
}
static int scene_driller_damage_receive(scene_driller_damage *owner,const rf_damage_request *request,
    float difficulty,uint32_t clock,uint32_t linked_player,const rf_damage_effect_backend *effects,
    scene_driller_destroyed destroyed,void *context,float *result)
{
    scene_driller_damage_context c={0};rf_damage_backend backend={scene_driller_damage_lookup,scene_driller_damage_predicate,scene_driller_damage_effect,&c};
    float value=0;int status;
    if(!owner || !request || !result || !effects || !destroyed || request->kind< -1 || request->kind>10 ||
       !effects->predicate || !effects->resolve_uid || !effects->source || !effects->create_burn || !effects->random ||
       !effects->notify || !effects->playing || !effects->play_kind6)return RF_RANGE;
    if(!isfinite(request->amount) || !isfinite(difficulty))return RF_FORMAT;
    if(owner->destruction_pending) {
        status=destroyed(context,owner->state.effects.handle,owner->state.responsible_handle);if(status)return status;
        owner->destruction_pending=0;
    }
    if(owner->destroyed){*result=0;return RF_OK;}
    c.owner=owner;c.effects=effects;c.clock=clock;c.linked_player=!!linked_player;
    c.object=(rf_damage_object){0,owner->object_flags,owner->state.effects.health};
    status=rf_damage_dispatch_sp(owner->state.effects.handle,request,difficulty,&backend,&value);
    owner->object_flags=c.object.flags;owner->state.effects.health=c.object.health;
    if(owner->state.effects.health<=0){owner->destroyed=1;owner->destruction_pending=1;}
    if(c.status)return c.status;
    if(status)return status;
    if(owner->destruction_pending) {
        status=destroyed(context,owner->state.effects.handle,owner->state.responsible_handle);if(status)return status;
        owner->destruction_pending=0;
    }
    *result=value;return RF_OK;
}
#endif
