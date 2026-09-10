#include "rf/level_particles.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int level_tick_valid(const rf_level_particles *p,const rf_visibility *v,float dt,rf_level_particle_tick_result *out)
{
    uint32_t i;
    if(!p || !p->state || !v || (v->count && !v->rooms) || !out || !isfinite(dt) || dt<0 ||
       p->materials.count>RF_PARTICLE_EMITTER_CAPACITY || (p->materials.count && !p->materials.bindings))return RF_RANGE;
    for(i=0;i<p->materials.count;i++) {
        const rf_emitter_slot *s=p->state->slots+i;
        if(!s->active || s->source_id!=p->materials.bindings[i].uid || s->runtime.emitter.room>v->count)return RF_RANGE;
    }
    memset(out,0,sizeof(*out));return RF_OK;
}
static int level_object(int32_t handle,rf_level_particle_lookup lookup,void *context,rf_level_particle_object *object)
{
    memset(object,0,sizeof(*object));object->uid=-1;
    if(handle<0)return RF_OK;
    if(lookup)return lookup(context,(uint32_t)handle,object);
    return handle==0?RF_OK:RF_NOT_FOUND;
}
int rf_level_particles_emit_pass(rf_level_particles *p,const rf_visibility *v,
    uint32_t enabled,float dt,int32_t now,rf_level_particle_lookup lookup,void *context,
    rf_level_particle_tick_result *out)
{
    uint32_t i;int status=level_tick_valid(p,v,dt,out);if(status)return status;
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<p->materials.count;i++) {
        rf_emitter_slot *slot=p->state->slots+i;rf_particle_emitter_update_result update;
        rf_level_particle_object object;uint32_t room=slot->runtime.emitter.room;
        if(!room || !(v->rooms[room-1].visible&255u))continue;
        status=level_object(slot->runtime.emitter.owner,lookup,context,&object);if(status)return status;
        if(object.found && object.room>v->count)return RF_RANGE;
        status=rf_particle_emitter_update(&p->state->particles,&slot->runtime,i+1,enabled,dt,now,
            object.found?&object.parent:NULL,object.room,&p->state->random,&update);if(status)return status;
        ++out->emitter_updates;out->created+=update.created;
    }
    return RF_OK;
}
static int level_step_list(rf_level_particles *p,const rf_visibility *v,uint32_t list,float dt,
    rf_level_particle_lookup lookup,void *context,rf_level_particle_tick_result *out)
{
    rf_particle_pool *pool=&p->state->particles;uint32_t index,visited=0;
    for(index=pool->lists[list].next;index!=RF_PARTICLE_CAPACITY+list;) {
        rf_particle *particle;rf_particle_owner_gate gate={0};rf_particle_emitter_bounds *bounds=NULL;
        rf_level_particle_object object;uint32_t next,i;int status;
        if(index>=RF_PARTICLE_CAPACITY || ++visited>RF_PARTICLE_CAPACITY)return RF_RANGE;
        particle=pool->particles+index;next=particle->next;
        if(particle->emitter) {
            if(particle->emitter>RF_PARTICLE_EMITTER_CAPACITY || !p->state->slots[particle->emitter-1].active)return RF_RANGE;
            bounds=&p->state->slots[particle->emitter-1].bounds;
        }
        if((int32_t)particle->owner>=0) {
            status=level_object((int32_t)particle->owner,lookup,context,&object);if(status)return status;
            for(i=0;i<p->materials.count;i++)if(p->materials.bindings[i].uid==(uint32_t)(object.found?object.uid:-1)) {
                uint32_t room=p->state->slots[i].runtime.emitter.room;
                gate.entry_found=1;gate.room_present=room!=0;gate.room_visible=room?v->rooms[room-1].visible:0;break;
            }
        }
        status=rf_particle_pool_step_resolved(pool,index,dt,bounds,&gate);if(status)return status;
        ++out->stepped;out->expired+=!(pool->particles[index].flags&1u);index=next;
    }
    return RF_OK;
}
int rf_level_particles_simulate(rf_level_particles *p,const rf_visibility *v,
    uint32_t enabled,float dt,rf_level_particle_lookup lookup,void *context,rf_level_particle_tick_result *out)
{
    uint32_t index,visited=0;int status=level_tick_valid(p,v,dt,out);if(status)return status;
    status=level_step_list(p,v,2,dt,lookup,context,out);if(status)return status;
    for(index=p->state->emitters.lists[1].next;index!=129;index=p->state->slots[index].next) {
        if(index>=RF_PARTICLE_EMITTER_CAPACITY || ++visited>RF_PARTICLE_EMITTER_CAPACITY)return RF_RANGE;
        status=level_step_list(p,v,RF_PARTICLE_BASE_LISTS+index,dt,lookup,context,out);if(status)return status;
    }
    status=level_step_list(p,v,4,dt,lookup,context,out);if(status)return status;
    return rf_emitter_pool_finish_bounds(&p->state->emitters,enabled);
}
void rf_level_particles_close(rf_level_particles *particles)
{
    if(!particles)return;
    rf_level_particle_materials_close(&particles->materials);
    free(particles->state);memset(particles,0,sizeof(*particles));
}
int rf_level_particles_open(rf_level_particles *particles,const rf_level *level,
    const rf_geometry_collision_world *world,rf_vpp *archives,uint32_t archive_count,
    uint32_t seed,int32_t now_ms,uint32_t budget)
{
    rf_level_particles value={0};rf_level_emitter_reader reader;rf_level_emitter record;
    uint32_t i,j;int status;
    if(!particles || !level || !world || (!archives && archive_count) || now_ms<0 || now_ms>RF_TIMER_PERIOD)return RF_RANGE;
    if((uint64_t)sizeof(value)+sizeof(*value.state)>budget)return RF_RANGE;
    value.resident_bytes=(uint32_t)(sizeof(value)+sizeof(*value.state));
    value.state=calloc(1,sizeof(*value.state));if(!value.state)return RF_RANGE;
    status=rf_particle_pool_init(&value.state->particles,value.state->records,value.state->lists,133);if(status)goto failed;
    status=rf_emitter_pool_init(&value.state->emitters,value.state->slots,&value.state->particles);if(status)goto failed;
    value.state->random.value=seed;
    status=rf_level_particle_materials_open(&value.materials,level,archives,archive_count,
        budget-value.resident_bytes+(uint32_t)sizeof(value.materials));if(status)goto failed;
    value.resident_bytes+=value.materials.resident_bytes-(uint32_t)sizeof(value.materials);
    status=rf_level_emitters_begin(level,&reader);
    if(status==RF_NOT_FOUND){*particles=value;return RF_OK;}
    if(status)goto failed;
    for(i=0;i<value.materials.count;i++) {
        rf_particle_emitter_template source={0};rf_collision_room_location location;
        uint32_t index,texture=value.materials.bindings[i].texture,room;
        status=rf_level_emitter_next(&reader,&record);if(status)goto failed;
        if(record.uid!=value.materials.bindings[i].uid){status=RF_FORMAT;goto failed;}
        for(j=0;j<i;j++)if(value.materials.bindings[j].uid==record.uid){status=RF_FORMAT;goto failed;}
        status=rf_geometry_collision_world_locate(world,record.position,&location);if(status)goto failed;
        room=location.room==UINT32_MAX?0:location.room+1;
        status=rf_level_emitter_template(&record,texture,value.materials.textures[texture].bitmap.frames,&source);if(status)goto failed;
        status=rf_emitter_pool_create(&value.state->emitters,&source,0,room,record.enabled!=0,now_ms,NULL,&value.state->random,&index);
        if(status)goto failed;
        if(index!=i){status=RF_FORMAT;goto failed;}
    }
    *particles=value;return RF_OK;
failed:
    rf_level_particles_close(&value);return status;
}
