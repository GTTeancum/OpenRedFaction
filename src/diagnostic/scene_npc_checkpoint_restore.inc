#ifndef RF_SCENE_NPC_CHECKPOINT_RESTORE_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_RESTORE_INCLUDED
#include "rf/npc_checkpoint.h"
#include "scene_npc_checkpoint_pair_fit.inc"
/* Read-only candidate-world placement/support and room resolver. Must fill a
 * fresh room state with query_position equal to placement.position. No gameplay
 * callbacks, allocation of live resources or actor publication are permitted. */
typedef int (*scene_npc_checkpoint_place)(void *,const rf_checkpoint_placement *,rf_entity_room_state *,rf_physics_support_contact *);
typedef struct scene_npc_checkpoint_restore_entry {
    uint32_t slot,handle,registered,flags;
    rf_npc_checkpoint_record saved,before;
    rf_physics_body_state body;rf_entity_room_state room;rf_physics_support_contact support;
    rf_entity_motion_selection selection;rf_motion_playback_state playback;
    float basis[9],published[3],eye[3];int32_t idle;
} scene_npc_checkpoint_restore_entry;
typedef struct scene_npc_checkpoint_restore_stage {
    uint32_t count,bytes;int32_t now;
    scene_npc_checkpoint_restore_entry entries[];
} scene_npc_checkpoint_restore_stage;
static int scene_npc_checkpoint_restore_placement(void *context,uint32_t index,
    rf_checkpoint_placement *p,uint32_t *retired)
{
    const scene_npc_checkpoint_restore_stage *stage=context;
    const scene_npc_checkpoint_restore_entry *e;
    if(!stage||index>=stage->count)return RF_RANGE;e=stage->entries+index;
    *retired=e->saved.retired;if(*retired)return RF_OK;
    if(e->slot>=campaign_npc_body_count)return RF_FORMAT;
    memset(p,0,sizeof(*p));p->spheres=campaign_npc_bodies[e->slot].body.spheres.items;
    p->count=campaign_npc_bodies[e->slot].body.spheres.count;
    memcpy(p->position,e->published,12);memcpy(p->basis,e->basis,36);return RF_OK;
}
static inline void scene_npc_checkpoint_restore_discard(scene_npc_checkpoint_restore_stage **stage)
{if(stage){free(*stage);*stage=NULL;}}
static int scene_npc_checkpoint_restore_eye(uint32_t slot,const scene_npc_checkpoint_restore_entry *e,float result[3])
{
    rf_eye_input input={0};const campaign_npc_eye_class *eye;const rf_entity_pose *pose;
    float tag[12],placed[12];int status;
    if(!campaign_npc_eyes||slot>=campaign_poses.count||!campaign_poses.items)return RF_RANGE;
    eye=campaign_npc_eyes+e->saved.class_id;pose=campaign_poses.items+slot;
    memcpy(input.position,e->published,12);memcpy(input.orientation,e->basis,36);
    memcpy(input.standing_offset,eye->offsets,12);memcpy(input.crouching_offset,eye->offsets+3,12);
    input.flags=campaign_seeds.classes[e->saved.class_id].physics.flags2;input.eye_tag=eye->tag;
    status=rf_eye_position(&input,result);if(status!=RF_NOT_FOUND)return status;
    if(eye->parent<0||(uint32_t)eye->parent>=pose->bone_count||!pose->matrices)return RF_RANGE;
    status=rf_model_compose_transform(eye->local,pose->matrices[eye->parent],tag);
    if(!status)status=rf_model_place_tag(tag,e->basis,e->published,placed);
    if(!status)memcpy(result,placed+9,12);return status;
}
static inline int scene_npc_checkpoint_restore_prepare(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,scene_npc_checkpoint_restore_stage **out)
{
    scene_npc_checkpoint_restore_stage *stage=NULL;rf_npc_checkpoint_record current;
    unsigned char identity[32]={0},wire[RF_NPC_CHECKPOINT_HEADER+RF_NPC_CHECKPOINT_ROW];
    uint32_t i,j,expected=0,written;uint64_t bytes;int status;
    if(!out||*out||!place||!catalog||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||!campaign_seeds.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    status=rf_npc_checkpoint_encode(identity,catalog,NULL,0,wire,sizeof(wire),&written);if(status)return status;
    for(i=0;i<campaign_npc_body_count;i++){
        status=scene_npc_checkpoint_row(i,now,&current);if(status==RF_NOT_FOUND)continue;if(status)return status;expected++;
    }
    if(expected!=count)return RF_FORMAT;
    bytes=sizeof(*stage)+(uint64_t)count*sizeof(*stage->entries);if(bytes>budget||bytes>UINT32_MAX)return RF_RANGE;
    stage=calloc(1,(size_t)bytes);if(!stage)return RF_IO;stage->count=count;stage->bytes=(uint32_t)bytes;stage->now=now;
    for(i=0;i<count;i++){
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body *o;
        const rf_entity_creation_vitals_class *vitals;rf_checkpoint_placement placement={0};
        rf_motion_playback_resource clip;rf_entity_playback_model *model;float angles[3]={0};int32_t id;uint32_t cls;
        status=rf_npc_checkpoint_encode(identity,catalog,rows+i,1,wire,sizeof(wire),&written);if(status)goto fail;
        if(i&&rows[i-1].uid>=rows[i].uid){status=RF_FORMAT;goto fail;}
        for(j=0;j<campaign_npc_body_count;j++)if(campaign_npc_bodies[j].persistence_registered&&
            (uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
        if(j==campaign_npc_body_count){status=RF_FORMAT;goto fail;}o=campaign_npc_bodies+j;cls=campaign_seeds.items[j].class_index;
        if(cls!=rows[i].class_id||cls>=campaign_seeds.class_count||!campaign_seeds.classes){status=RF_FORMAT;goto fail;}
        vitals=&campaign_seeds.classes[cls].vitals;
        if(rows[i].health>(vitals->health<0?100:vitals->health)||rows[i].armor>vitals->armor){status=RF_FORMAT;goto fail;}
        e->slot=j;e->handle=o->registration.handle;e->registered=o->registration.view!=NULL;e->saved=rows[i];e->idle=-1;
        status=scene_npc_checkpoint_row(j,now,&e->before);if(status)goto fail;
        if(!rows[i].retired&&!e->registered){status=RF_RANGE;goto fail;} /* Fresh actors, never resurrect freed bodies. */
        e->flags=(o->object_flags&~0x4004u)|rows[i].flags;
        memcpy(e->published,rows[i].position,12);angles[1]=rows[i].yaw;
        status=rf_look_orientation(angles,e->basis);if(status)goto fail;
        rf_motion_playback_initialize(&e->playback);
        if(rows[i].retired)continue;
        if(!campaign_model_owners||j>=campaign_model_owner_count||!campaign_poses.items||j>=campaign_poses.count){status=RF_RANGE;goto fail;}
        e->body=o->body.state;memcpy(e->body.position,rows[i].position,12);memcpy(e->body.next_position,rows[i].position,12);
        memcpy(e->body.orientation,e->basis,36);memcpy(e->body.next_orientation,e->basis,36);memset(e->body.velocity,0,12);
        status=rf_physics_spheres_bounds(o->body.spheres.items,o->body.spheres.count,e->body.position,&e->body.bounds);if(status)goto fail;
        status=rf_physics_tensor_world(e->body.local_tensor,e->basis,e->body.world_tensor);if(status)goto fail;
        status=rf_physics_publish_position(&e->body,e->published,&e->flags);if(status)goto fail;
        placement.spheres=o->body.spheres.items;placement.count=o->body.spheres.count;
        placement.query_flags=(o->body.state.state_124&~0x1000u)|4u;
        memcpy(placement.position,e->published,12);memcpy(placement.basis,e->basis,36);
        status=place(context,&placement,&e->room,&e->support);if(status)goto fail;
        if(memcmp(e->room.query_position,e->published,12)||e->support.handle||e->support.material<0){status=RF_FORMAT;goto fail;}e->room.flags=e->flags;
        status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&e->selection);if(status)goto fail;
        if(rows[i].primary>=0){status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,rows[i].primary,&e->selection);if(status)goto fail;}
        if(e->selection.mapping.skeleton!=campaign_poses.items[j].skeleton){status=RF_FORMAT;goto fail;}
        if(campaign_poses.items[j].controller.override_enabled||campaign_poses.items[j].controller.duration!=0||
           campaign_poses.items[j].controller.current>0||campaign_poses.items[j].controller.next>0){status=RF_RANGE;goto fail;}
        id=e->selection.mapping.states[0];
        if(id>=0){
            uint32_t skeleton=e->selection.mapping.skeleton,cache;
            if(!campaign_playback_resources.models||skeleton>=campaign_playback_resources.model_count||
               !campaign_motion_catalog.models||skeleton>=campaign_motion_catalog.model_count){status=RF_RANGE;goto fail;}
            model=campaign_playback_resources.models+skeleton;
            if(!model->resources||!model->cache_ids||(uint32_t)id>=model->count||
               (uint32_t)id>=campaign_motion_catalog.models[skeleton].count||!campaign_motion_catalog.models[skeleton].items){status=RF_RANGE;goto fail;}
            cache=model->cache_ids[id];
            if(!campaign_npc_motion_data||cache>=campaign_npc_motion_count||!campaign_npc_motion_data[cache]||
               campaign_motion_catalog.models[skeleton].items[id].file.resident!=campaign_npc_motion_data[cache]||
               !model->resources[id].looping||model->resources[id].references>INT32_MAX-(int32_t)count){status=RF_RANGE;goto fail;}
            clip=model->resources[id];status=rf_motion_set_weight(&e->playback,&clip,1,0,1);if(status)goto fail;
            e->playback.completion.active.slots[0].motion=id;e->playback.completion.active.slots[0].tick=clip.comparison.start_tick;e->idle=id;
        }
        status=scene_npc_checkpoint_restore_eye(j,e,e->eye);if(status)goto fail;
    }
    status=scene_npc_checkpoint_pair_fit(count,scene_npc_checkpoint_restore_placement,stage);if(status)goto fail;
    *out=stage;return RF_OK;
fail:free(stage);return status;
}
/* All fallible work finishes before the first actor assignment. The caller
 * keeps scene/resource ownership stable between prepare and commit. */
static inline int scene_npc_checkpoint_restore_commit(scene_npc_checkpoint_restore_stage *stage)
{
    uint32_t i,j;int status;rf_npc_checkpoint_record current;
    if(!stage)return RF_RANGE;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        if(e->slot>=campaign_npc_body_count||campaign_npc_bodies[e->slot].registration.handle!=e->handle)return RF_FORMAT;
        status=scene_npc_checkpoint_row(e->slot,stage->now,&current);if(status)return status;
        if(memcmp(&current,&e->before,sizeof(current)))return RF_FORMAT;
    }
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body *o=campaign_npc_bodies+e->slot;
        uint32_t persistence=o->persistence_slot;rf_entity_pose *pose=NULL;
        if(e->registered){
            pose=campaign_poses.items+e->slot;
            for(j=0;j<pose->playback.completion.active.count;j++){
                int32_t id=pose->playback.completion.active.slots[j].motion;
                rf_motion_playback_resource *r=campaign_playback_resources.models[pose->skeleton].resources+id;
                if(r->references>0)r->references--;
            }
            if(e->idle>=0)campaign_playback_resources.models[pose->skeleton].resources[e->idle].references++;
            pose->playback=e->playback;
            memset(&pose->controller,0,sizeof(pose->controller));pose->controller.override_state=-1;
        }
        o->inventory=e->saved.inventory;o->view.weapons[0]=e->saved.primary;o->view.weapons[1]=e->saved.secondary;
        o->damage.effects.health=e->saved.health;o->damage.effects.armor=e->saved.armor;o->damage.effects.affiliation=e->saved.affiliation;
        o->ai_mode.action_280=e->saved.ai_mode;o->ai_mode.clock_288=stage->now;
        o->object_flags=e->flags;memcpy(o->published,e->published,12);memcpy(o->previous,e->published,12);
        memset(o->look.body_angles,0,12);o->look.body_angles[1]=e->saved.yaw;memcpy(o->look.orientation,e->basis,36);
        rf_scene_defeated_actors.items[persistence].retired=e->saved.retired;
        rf_scene_defeated_actors.vitals[persistence].valid=!e->saved.retired;
        rf_scene_defeated_actors.vitals[persistence].health=e->saved.health;rf_scene_defeated_actors.vitals[persistence].armor=e->saved.armor;
        rf_scene_defeated_actors.mission[persistence].affiliation=e->saved.affiliation;rf_scene_defeated_actors.mission[persistence].flags=e->saved.flags;
        rf_scene_defeated_actors.drops[persistence]=e->saved.drop;
        if(e->saved.retired){
            if(e->registered)(void)rf_entity_view_unregister(&campaign_registry,&campaign_entities,&o->registration);
            rf_physics_body_close(&o->body);o->object_flags|=2|0x4000u;o->view.flags_810|=1;o->damage.effects.flags_810=o->view.flags_810;
        }else{
            o->body.state=e->body;o->room=e->room;o->support=e->support;o->selection=e->selection;memcpy(o->eye_position,e->eye,12);
            memcpy(campaign_model_owners[e->slot].position,e->published,12);memcpy(campaign_model_owners[e->slot].basis,e->basis,36);
            campaign_model_owners[e->slot].room=e->room.room?e->room.room-1:UINT32_MAX;
        }
        o->view.flags_7c=o->object_flags;
    }
    return RF_OK;
}
#endif
