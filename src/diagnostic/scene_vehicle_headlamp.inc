#ifndef RF_SCENE_VEHICLE_HEADLAMP_INCLUDED
#define RF_SCENE_VEHICLE_HEADLAMP_INCLUDED
/* Original ON4b92e0->429560/OFF4ba0e0->4294a0: entity810 bit4000,
 * then only attached glare2b4 bit2 children. 42d8d0/e0 set active28c.
 * Entity-table corona classes and model corona_N tags feed real type10
 * owners through the existing413d20 reconstruction. No projected light,
 * replacement vehicle/actor or guessed lamp geometry is introduced. */
typedef struct scene_vehicle_headlamp_binding {
    uint32_t uid,handle,initial,first,count;
} scene_vehicle_headlamp_binding;
static scene_vehicle_headlamp_binding *scene_vehicle_headlamps;
static uint32_t scene_vehicle_headlamp_count;
uint32_t rf_scene_vehicle_headlamp[12]; /* owners,children,calls,on,off,UID,handle,810,poses,enabled,bytes,status */
typedef struct scene_vehicle_headlamp_view {
    rf_damage_effect_state *effects;rf_entity_view *entity;
    const scene_driller_resources *resource;const float *position,*basis;
    uint32_t uid,flags;int32_t parent;
} scene_vehicle_headlamp_view;
static int scene_vehicle_headlamp_view_read(scene_stream *s,uint32_t handle,scene_vehicle_headlamp_view *out)
{
    scene_driller_damage_runtime *a=scene_driller_damage_owner;scene_vehicle_headlamp_view v={0};
    if(!s||!out)return RF_RANGE;v.parent=-1;
    if(scene_driller_damage_runtime_valid(a)&&a->damage.state.effects.handle==handle&&
       s->driller_runtime&&a==&s->driller_runtime->damage&&
       rf_object_registry_lookup(&campaign_registry,handle)==&a->entry->registration){
        v.effects=&a->damage.state.effects;v.entity=&a->entry->view;v.resource=s->driller;
        v.position=s->driller_runtime->physics.state.position;v.basis=s->driller_runtime->physics.state.orientation;
        v.uid=(uint32_t)campaign_authored_vehicle_uid;v.flags=a->damage.object_flags;v.parent=v.entity->linked_handle;
    }else for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        scene_passive_vehicle *p=campaign_passive_vehicles+i;
        if(p->handle!=handle||p->damage.state.effects.handle!=handle||
           rf_object_registry_lookup(&campaign_registry,handle)!=p)continue;
        if(p->resource_kind>=6)return RF_FORMAT;
        v.effects=&p->damage.state.effects;v.position=p->pose.position;v.basis=p->pose.input_matrix;
        v.resource=campaign_selected_vehicle_resource_kind()==p->resource_kind?s->driller:s->passive_vehicle_resources[p->resource_kind];
        v.uid=p->uid;v.flags=p->damage.object_flags;break;
    }
    if(!v.effects)return RF_NOT_FOUND;
    if(!v.resource)return RF_NOT_FOUND;*out=v;return RF_OK;
}
static uint32_t scene_vehicle_headlamp_parent(uint32_t handle)
{
    for(uint32_t i=0;i<scene_vehicle_headlamp_count;i++)if(scene_vehicle_headlamps[i].handle==handle)return 1;
    return 0;
}
static uint32_t scene_vehicle_headlamp_initial(uint32_t handle)
{
    for(uint32_t i=0;i<scene_vehicle_headlamp_count;i++)if(scene_vehicle_headlamps[i].handle==handle)
        return scene_vehicle_headlamps[i].initial?0x4000u:0u;
    return 0; /* No authored source in the explicit developer host. */
}
static void scene_vehicle_headlamp_close(void)
{
    free(scene_vehicle_headlamps);scene_vehicle_headlamps=NULL;scene_vehicle_headlamp_count=0;
    /* Glare children are owned and retired by campaign_glare_instances_close. */
}
static int scene_vehicle_headlamp_pose(void *context,uint32_t handle,int32_t tag,float pose[12])
{
    scene_vehicle_headlamp_view v;int status=scene_vehicle_headlamp_view_read(context,handle,&v);
    if(status)return status;
    return rf_static_model_tag_place(&v.resource->tags,tag,v.basis,v.position,pose);
}
static void scene_vehicle_headlamp_children(uint32_t handle,uint32_t enabled)
{
    for(uint32_t i=0;i<scene_vehicle_headlamp_count;i++){
        const scene_vehicle_headlamp_binding *b=scene_vehicle_headlamps+i;if(b->handle!=handle)continue;
        for(uint32_t j=0;j<b->count;j++){
            rf_glare_base_owner *g=campaign_glare_instances[b->first+j];
            if(g&&g->state.parent==handle&&(g->state.flags&2u))g->state.active=(uint8_t)enabled;
        }
    }
}
static int scene_vehicle_headlamp_set(void *context,uint32_t handle,uint32_t enabled)
{
    scene_stream *s=context;uint32_t visits=0;scene_vehicle_headlamp_view v;int status;
    if(enabled>1)return RF_RANGE;
    /* The original follows entity200, not moving-group memberships or seats.
     * Installed vehicle targets have no entity parent. Complete-handle lookup
     * still guards each represented vehicle ancestor and bounds bad cycles. */
    while(visits<RF_OBJECT_CAPACITY){
        status=scene_vehicle_headlamp_view_read(s,handle,&v);
        if(status==RF_NOT_FOUND)return visits?RF_OK:status;if(status)return status;
        v.effects->flags_810=(v.effects->flags_810&~0x4000u)|(enabled?0x4000u:0u);
        if(v.entity)v.entity->flags_810=(v.entity->flags_810&~0x4000u)|(v.effects->flags_810&0x4000u);
        scene_vehicle_headlamp_children(handle,enabled);
        ++rf_scene_vehicle_headlamp[2];++rf_scene_vehicle_headlamp[enabled?3:4];
        rf_scene_vehicle_headlamp[5]=v.uid;rf_scene_vehicle_headlamp[6]=handle;rf_scene_vehicle_headlamp[7]=v.effects->flags_810;
        ++visits;if(v.parent<0)return RF_OK;handle=(uint32_t)v.parent;
    }
    return RF_FORMAT;
}
static int scene_vehicle_headlamp_open(scene_stream *s,const char *tables_path)
{
    rf_vpp tables={0};rf_vpp_entry entry;void *text=NULL;rf_glare_base_owner **grown=NULL;
    uint32_t count=campaign_passive_vehicle_count+(campaign_authored_vehicle_uid!=0),capacity,bytes;uint64_t extra;int status=RF_OK;
    if(!s||!tables_path||scene_vehicle_headlamps||campaign_attachment_nodes||campaign_glare_rooms)return RF_RANGE;
    memset(rf_scene_vehicle_headlamp,0,sizeof(rf_scene_vehicle_headlamp));if(!count)return RF_OK;
    if(count>RF_OBJECT_CAPACITY)return RF_RANGE;
    scene_vehicle_headlamps=calloc(count,sizeof(*scene_vehicle_headlamps));if(!scene_vehicle_headlamps)return RF_IO;
    scene_vehicle_headlamp_count=count;capacity=campaign_glare_instance_capacity;
    for(uint32_t i=0;i<count;i++){
        scene_vehicle_headlamp_binding *b=scene_vehicle_headlamps+i;scene_vehicle_headlamp_view v;
        uint32_t slot=0,matches=0;
        b->handle=campaign_authored_vehicle_uid&&!i?campaign_authored_vehicle_handle:
            campaign_passive_vehicles[i-(campaign_authored_vehicle_uid!=0)].handle;
        status=scene_vehicle_headlamp_view_read(s,b->handle,&v);if(status)goto done;b->uid=v.uid;
        for(uint32_t j=0;j<campaign_seeds.records.count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==b->uid){slot=j;++matches;}
        if(matches!=1){status=RF_FORMAT;goto done;}
        status=rf_level_entity_headlamp_read(campaign_seeds.records.items+slot,&b->initial);if(status)goto done;
        if(v.resource->tags.count>UINT32_MAX-capacity){status=RF_RANGE;goto done;}
        capacity+=v.resource->tags.count;
        v.effects->flags_810=(v.effects->flags_810&~0x4000u)|(b->initial?0x4000u:0u);
        if(v.entity)v.entity->flags_810=(v.entity->flags_810&~0x4000u)|(v.effects->flags_810&0x4000u);
    }
    extra=(uint64_t)(capacity-campaign_glare_instance_capacity)*sizeof(*grown)+(uint64_t)count*sizeof(*scene_vehicle_headlamps);
    if(rf_scene_glare_instances[5]>256u*1024u||extra>256u*1024u-rf_scene_glare_instances[5]){status=RF_RANGE;goto done;}
    bytes=(uint32_t)extra;
    grown=calloc(capacity,sizeof(*grown));if(!grown){status=RF_IO;goto done;}
    if(campaign_glare_instance_count)memcpy(grown,campaign_glare_instances,campaign_glare_instance_count*sizeof(*grown));
    free(campaign_glare_instances);campaign_glare_instances=grown;grown=NULL;campaign_glare_instance_capacity=capacity;
    rf_scene_glare_instances[5]+=bytes;rf_scene_vehicle_headlamp[10]=bytes;
    if(rf_scene_glare_instances[5]>rf_scene_glare_instances[6])rf_scene_glare_instances[6]=rf_scene_glare_instances[5];
    status=rf_vpp_open(&tables,tables_path);if(status)goto done;
    status=rf_vpp_find(&tables,"entity.tbl",&entry);if(status)goto done;
    if(!entry.size||entry.size>512u*1024u){status=RF_RANGE;goto done;}
    text=malloc(entry.size);if(!text){status=RF_IO;goto done;}
    status=rf_vpp_read(&tables,&entry,0,text,entry.size);if(status)goto done;
    for(uint32_t i=0;i<count;i++){
        scene_vehicle_headlamp_binding *b=scene_vehicle_headlamps+i;scene_vehicle_headlamp_view v;uint32_t slot=0;
        status=scene_vehicle_headlamp_view_read(s,b->handle,&v);if(status)goto done;
        while(slot<campaign_seeds.records.count&&(uint32_t)campaign_seeds.records.items[slot].record.uid!=b->uid)++slot;
        if(slot==campaign_seeds.records.count){status=RF_FORMAT;goto done;}
        b->first=campaign_glare_instance_count;
        for(uint32_t ordinal=1;ordinal<=v.resource->tags.count;ordinal++){
            char name[64],tag_name[32];uint32_t headlamp,cls;int32_t tag;rf_glare_base_owner *glare=NULL;
            rf_glare_services services={scene_vehicle_headlamp_pose,s};
            const rf_entity_material *m=campaign_surface_palette->materials;float material[3]={m->elasticity,m->friction,m->density};
            status=rf_entity_corona_read(text,entry.size,campaign_seeds.records.items[slot].record.class_name,ordinal,name,&headlamp);
            if(status==RF_NOT_FOUND){status=RF_OK;break;}if(status)goto done;
            snprintf(tag_name,sizeof(tag_name),"corona_%u",ordinal);
            status=rf_static_model_tags_find(&v.resource->tags,(rf_model_name){tag_name,strlen(tag_name)},&tag);
            if(status==RF_NOT_FOUND){status=RF_OK;break;}if(status)goto done;
            if(!headlamp||!*name)continue;
            for(cls=0;cls<campaign_glare_classes.count;cls++)if(!strcmp(campaign_glare_classes.definitions[cls].name,name))break;
            if(cls==campaign_glare_classes.count){status=RF_NOT_FOUND;goto done;}
            if(campaign_glare_instance_count==capacity||rf_scene_glare_instances[5]>=256u*1024u){status=RF_RANGE;goto done;}
            status=rf_glare_owned_open(campaign_glare_classes.items,campaign_glare_classes.count,(int32_t)cls,
                b->handle,tag,1,&campaign_registry,&campaign_clutter_objects,&campaign_glare_list,&campaign_clutter_uid_cursor,
                campaign_seeds.items[slot].spawn.byte_28,0,material,256u*1024u-rf_scene_glare_instances[5],&services,&glare);
            if(status)goto done;if(!glare)continue; /* Original factory allows exhausted registry. */
            glare->state.active=(uint8_t)b->initial;
            campaign_glare_instances[campaign_glare_instance_count++]=glare;++b->count;
            rf_scene_glare_instances[5]+=glare->allocated_bytes;rf_scene_vehicle_headlamp[10]+=glare->allocated_bytes;
            if(rf_scene_glare_instances[5]>rf_scene_glare_instances[6])rf_scene_glare_instances[6]=rf_scene_glare_instances[5];
            ++rf_scene_vehicle_headlamp[1];++rf_scene_glare_instances[3];
        }
        ++rf_scene_vehicle_headlamp[0];
    }
done:
    free(text);free(grown);rf_vpp_close(&tables);rf_scene_vehicle_headlamp[11]=(uint32_t)status;
    return status; /* Scene teardown owns partially created real children. */
}
static int scene_vehicle_headlamp_tick(scene_stream *s)
{
    int status;rf_scene_vehicle_headlamp[9]=0;
    for(uint32_t i=0;i<scene_vehicle_headlamp_count;i++){
        const scene_vehicle_headlamp_binding *b=scene_vehicle_headlamps+i;scene_vehicle_headlamp_view v;
        status=scene_vehicle_headlamp_view_read(s,b->handle,&v);if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(v.uid!=b->uid)return RF_FORMAT;
        for(uint32_t j=0;j<b->count;j++){
            rf_glare_base_owner *g=campaign_glare_instances[b->first+j];float pose[12];if(!g)continue;
            if(g->state.parent!=b->handle||rf_object_registry_lookup(&campaign_registry,g->handle)!=&g->state)return RF_FORMAT;
            status=rf_static_model_tag_place(&v.resource->tags,g->state.tag,v.basis,v.position,pose);if(status)return status;
            status=rf_glare_publish_tag_pose(g,pose);if(status)return status;
            g->state.active=(uint8_t)!!(v.effects->flags_810&0x4000u);
            ++rf_scene_vehicle_headlamp[8];rf_scene_vehicle_headlamp[9]+=g->state.active;
        }
    }
    return RF_OK;
}
/* Assignment-only after all ordinary vehicle state publication. New scenes
 * rebuilt children from authored metadata; no glare handles/caches are saved. */
static void scene_vehicle_headlamp_assign(scene_stream *s)
{
    for(uint32_t i=0;i<scene_vehicle_headlamp_count;i++){
        scene_vehicle_headlamp_view v;uint32_t handle=scene_vehicle_headlamps[i].handle;
        if(scene_vehicle_headlamp_view_read(s,handle,&v))continue;
        if(v.entity)v.entity->flags_810=(v.entity->flags_810&~0x4000u)|(v.effects->flags_810&0x4000u);
        scene_vehicle_headlamp_children(handle,!!(v.effects->flags_810&0x4000u));
    }
}
static uint32_t scene_vehicle_headlamp_legacy_save_allowed(scene_stream *s)
{
    for(uint32_t i=0;i<scene_vehicle_headlamp_count;i++){
        const scene_vehicle_headlamp_binding *b=scene_vehicle_headlamps+i;scene_vehicle_headlamp_view v;
        if(scene_vehicle_headlamp_view_read(s,b->handle,&v)||v.uid!=b->uid||
           !!(v.effects->flags_810&0x4000u)!=b->initial)return 0;
    }
    return 1;
}
static int scene_vehicle_headlamp_visible(scene_stream *s,uint32_t handle,uint32_t *visible)
{
    scene_vehicle_headlamp_view v;rf_collision_room_location room;int status;
    if(!visible)return RF_RANGE;
    status=scene_vehicle_headlamp_view_read(s,handle,&v);if(status)return status;
    *visible=!(v.flags&(2u|0x4000u));if(!*visible||!s->visibility.storage)return RF_OK;
    status=rf_geometry_collision_world_locate(s->collision,v.position,&room);if(status)return status;
    if(room.room<s->visibility.state.count)*visible=s->visibility.state.rooms[room.room].visible!=0;
    return RF_OK;
}
#endif
