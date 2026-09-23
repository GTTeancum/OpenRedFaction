#ifndef RF_SCENE_WORLD_RESTORE_INCLUDED
#define RF_SCENE_WORLD_RESTORE_INCLUDED
#include "rf/world_checkpoint.h"
/* Include after NPC/mover/clutter restore and scene_checkpoint_world helpers.
 * This is a dependency stage, NOT a complete loader or publication service.
 * Geometry, metadata and resource ownership stay stable until discard. Caller
 * supplies the candidate static terrain; player, events, histories, vehicles,
 * remote charges and destruction need their own semantic stages/admission.
 * In particular, semantic_mask below is intentionally only three components.
 * The envelope input is borrowed unchanged for the stage lifetime. */
typedef struct scene_world_restore_stage {
    rf_world_checkpoint envelope;
    uint32_t semantic_mask,allocated_bytes;
    scene_mover_checkpoint_stage movers;
    scene_clutter_restore_stage *clutter;
    scene_npc_checkpoint_restore_stage *npcs;
    rf_geometry_collision_movers candidate_movers;
    rf_clutter_base_owner *candidate_props;
    rf_clutter_base_owner **prop_views,**authored_prop_views;
    rf_collision_body_mover *scratch;
    scene_checkpoint_world_context world;
    const rf_npc_checkpoint_record *placement_rows;uint32_t placement_count,placement_next;
} scene_world_restore_stage;
static inline int scene_world_event_probe(scene_stream *,const scene_world_restore_stage *,int32_t);

static inline void scene_world_restore_discard(scene_world_restore_stage **out)
{
    scene_world_restore_stage *s;
    if(!out||!*out)return;s=*out;
    scene_npc_checkpoint_restore_discard(&s->npcs);scene_mover_checkpoint_close(&s->movers);
    free(s->clutter);free(s->candidate_props);free(s->prop_views);free(s->authored_prop_views);free(s->scratch);free(s);*out=NULL;
}
static inline int scene_world_restore_failure(const char *phase,int status)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");
    if(enabled&&!strcmp(enabled,"1"))printf("WORLD_RESTORE_STAGE phase%s status%d\n",phase,status);
#else
    (void)phase;
#endif
    return status;
}
static inline int scene_world_restore_place(void *context,const rf_checkpoint_placement *placement,
    rf_entity_room_state *room,rf_physics_support_contact *support)
{
    scene_world_restore_stage *s=context;scene_checkpoint_world_context world=s->world;
    rf_collision_room_location location;uint32_t i,uid=UINT32_MAX;int status;
    while(s->placement_next<s->placement_count&&s->placement_rows[s->placement_next].retired)s->placement_next++;
    if(s->placement_next>=s->placement_count)return RF_FORMAT;
    uid=s->placement_rows[s->placement_next].uid;
    if(memcmp(s->placement_rows[s->placement_next].position,placement->position,12))return RF_FORMAT;
    s->placement_next++;
    world.authored_position=NULL;world.authored_basis=NULL;world.authored_rotating_npc=0;
    /* Immutable sphere-owner binding identifies the class, without consulting
     * live positions. Empty sets take the point-only room path (speed unused). */
    for(i=0;i<campaign_npc_body_count;i++)if(placement->count&&
        placement->spheres==campaign_npc_bodies[i].body.spheres.items){
        uint32_t cls=campaign_seeds.items[i].class_index;
        if(uid!=(uint32_t)campaign_seeds.records.items[i].record.uid)return RF_FORMAT;
        if(!campaign_npc_movement_configs||cls>=campaign_seeds.class_count)return RF_FORMAT;
        /* RFNC admission already requires zero motion. Bit1 marks falling;
         * 0x400000 owns moving support. Neither state may lose its contact. */
        world.allow_no_contact=!(campaign_npc_bodies[i].body.state.flags&(1u|0x400000u))&&
            !campaign_npc_bodies[i].support.handle;
        if(world.allow_no_contact&&world.authored_static_unchanged&&
           !s->envelope.sections[RF_WORLD_DESTRUCTION-1].bytes){
            world.authored_position=campaign_seeds.records.items[i].record.position;
            world.authored_basis=campaign_seeds.records.items[i].record.orientation[0];
            world.authored_rotating_npc=!strcmp(campaign_seeds.records.items[i].record.class_name,"Auto Turret");
        }
        world.class_speed=campaign_npc_movement_configs[cls].base_speed;break;
    }
    status=rf_geometry_collision_world_locate(world.world,placement->position,&location);
    if(!status){
        if(location.room==UINT32_MAX||location.room>=world.world->room_count)status=RF_NOT_FOUND;
        else {world.reference_room=location.room;status=scene_checkpoint_world_place(&world,placement,room,support);}
    }
#ifndef RF_IMAGE_XBOX_NATIVE
    if(status){const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");
        if(enabled&&!strcmp(enabled,"1")){
            printf("WORLD_RESTORE_NPC_PLACEMENT uid%u count%u position%.9g,%.9g,%.9g status%d\n",
                uid,placement->count,(double)placement->position[0],(double)placement->position[1],(double)placement->position[2],status);
            for(i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){
                const campaign_npc_body *owner=campaign_npc_bodies+i;uint32_t k;double basis_delta=0;
                for(k=0;k<9;k++){double delta=fabs((double)placement->basis[k]-owner->body.state.orientation[k]);if(delta>basis_delta)basis_delta=delta;}
                printf("WORLD_RESTORE_NPC_BODY uid%u class%s body_flags%u object_flags%u support%u material%d basis_delta%.9g authored_y%.9g current_y%.9g\n",
                    uid,campaign_seeds.records.items[i].record.class_name,owner->body.state.flags,owner->object_flags,
                    owner->support.handle,owner->support.material,basis_delta,(double)campaign_seeds.records.items[i].record.position[1],(double)owner->body.state.position[1]);
                for(k=0;k<placement->count;k++)printf("WORLD_RESTORE_NPC_SPHERE uid%u index%u center%.9g,%.9g,%.9g radius%.9g\n",uid,k,
                    (double)placement->spheres[k].center[0],(double)placement->spheres[k].center[1],(double)placement->spheres[k].center[2],(double)placement->spheres[k].radius);
                break;
            }
        }}
#else
    (void)uid;
#endif
    return status;
}
/* Decode and validate component identities before allocations. Prepare mover
 * poses, then static prop flags, then fit NPC candidates against that candidate
 * world. Never fit against the old live door/retired-prop state. Failure leaves
 * output and live gameplay unchanged. Budget includes temporary decoded rows
 * and private staged collision scratch, not caller-owned envelope/resources. */
static inline int scene_world_restore_prepare(const void *data,uint32_t bytes,
    const unsigned char identity[32],uint32_t required_mask,const rf_npc_checkpoint_catalog *catalog,
    int32_t now,const scene_checkpoint_world_context *world,uint32_t budget,scene_world_restore_stage **out)
{
    const uint32_t mask=RF_WORLD_CHECKPOINT_MASK(RF_WORLD_NPC)|RF_WORLD_CHECKPOINT_MASK(RF_WORLD_MOVER)|RF_WORLD_CHECKPOINT_MASK(RF_WORLD_CLUTTER);
    rf_world_checkpoint envelope;scene_world_restore_stage *s=NULL;
    rf_mover_checkpoint_record *mover_rows=NULL;rf_npc_checkpoint_record *npc_rows=NULL;
    const rf_world_checkpoint_slice *npc,*mover,*clutter;
    unsigned char clutter_identity[32];uint32_t npc_count,mover_count,prop_count,i,slot;
    uint64_t fixed,temp,resident;const char *phase="allocation";int status;
    if(!out||*out||!identity||!catalog||!world||!world->world||!world->metadata||
       now<0||now>RF_TIMER_PERIOD||campaign_movers.count>RF_OBJECT_CAPACITY)return RF_RANGE;
    status=rf_world_checkpoint_decode(data,bytes,required_mask|mask,&envelope);if(status)return scene_world_restore_failure("envelope",status);
    if(memcmp(envelope.identity,identity,32)||!scene_npc_checkpoint_level_equal(envelope.level,campaign_current_level))return RF_FORMAT;
    npc=envelope.sections+RF_WORLD_NPC-1;mover=envelope.sections+RF_WORLD_MOVER-1;clutter=envelope.sections+RF_WORLD_CLUTTER-1;
    status=rf_npc_checkpoint_preflight(npc->data,npc->bytes,identity,catalog,&npc_count);if(status)return scene_world_restore_failure("npc_codec",status);
    status=rf_mover_checkpoint_preflight(mover->data,mover->bytes,identity,&mover_count);if(status)return scene_world_restore_failure("mover_codec",status);
    status=scene_clutter_checkpoint_identity(identity,clutter_identity);if(status)return scene_world_restore_failure("clutter_identity",status);
    status=rf_clutter_checkpoint_preflight(clutter->data,clutter->bytes,clutter_identity,&prop_count);if(status)return scene_world_restore_failure("clutter_codec",status);
    if(prop_count>campaign_clutter_records.count)return RF_FORMAT;
    fixed=sizeof(*s)+(uint64_t)prop_count*(sizeof(*s->candidate_props)+2*sizeof(*s->prop_views))+
        (uint64_t)campaign_movers.count*sizeof(*s->scratch);
    resident=sizeof(scene_clutter_restore_stage)+(uint64_t)prop_count*sizeof(scene_clutter_restore_entry);
    temp=(uint64_t)prop_count*sizeof(rf_clutter_checkpoint_record);
    if(fixed+resident+temp>budget||fixed+resident>UINT32_MAX)return RF_RANGE;
    s=calloc(1,sizeof(*s));if(!s)return RF_IO;s->envelope=envelope;s->allocated_bytes=(uint32_t)fixed;
    if(prop_count){s->candidate_props=calloc(prop_count,sizeof(*s->candidate_props));s->prop_views=calloc(prop_count,sizeof(*s->prop_views));s->authored_prop_views=calloc(prop_count,sizeof(*s->authored_prop_views));}
    if(campaign_movers.count)s->scratch=calloc(campaign_movers.count,sizeof(*s->scratch));
    if((prop_count&&(!s->candidate_props||!s->prop_views||!s->authored_prop_views))||(campaign_movers.count&&!s->scratch)){status=RF_IO;goto fail;}
    phase="clutter_stage";status=scene_clutter_checkpoint_prepare(identity,clutter->data,clutter->bytes,now,0,&s->clutter);if(status)goto fail;
    s->allocated_bytes+=(uint32_t)resident;
    if(s->clutter->count!=prop_count){status=RF_FORMAT;goto fail;}
    for(i=0;i<prop_count;i++){
        const scene_clutter_restore_entry *e=s->clutter->entries+i;slot=e->slot;
        if(slot>=campaign_clutter_records.count||!campaign_clutter_bodies[slot]){status=RF_FORMAT;goto fail;}
        s->candidate_props[i]=*campaign_clutter_bodies[slot];s->prop_views[i]=s->candidate_props+i;
        /* Clutter identity/prepare has proved these are unchanged authored
         * positions/bases and retained class geometry, never saved moved props. */
        s->authored_prop_views[i]=campaign_clutter_bodies[slot];
        s->candidate_props[i].state.health=e->row.health;
        s->candidate_props[i].state.flags=(s->candidate_props[i].state.flags&~0x204002u)|e->row.flags;
    }
    temp=(uint64_t)mover_count*sizeof(*mover_rows);
    if((uint64_t)s->allocated_bytes+temp>budget){status=RF_RANGE;goto fail;}
    if(mover_count){mover_rows=malloc((size_t)temp);if(!mover_rows){status=RF_IO;goto fail;}}
    phase="mover_stage";status=rf_mover_checkpoint_decode(mover->data,mover->bytes,identity,mover_rows,mover_count,&mover_count);if(status)goto fail;
    status=scene_mover_checkpoint_prepare(&campaign_group_runtime,&campaign_group_registration,campaign_controller_views,
        &campaign_movers,mover_rows,mover_count,now,budget-s->allocated_bytes-(uint32_t)temp,&s->movers);if(status)goto fail;
    s->allocated_bytes+=s->movers.allocated_bytes;free(mover_rows);mover_rows=NULL;
    s->candidate_movers=campaign_movers;s->candidate_movers.poses=s->movers.poses;s->candidate_movers.views=s->movers.views;
    s->world=*world;s->world.movers=&s->candidate_movers;s->world.props=s->prop_views;s->world.prop_count=prop_count;
    s->world.authored_props=s->authored_prop_views;
    s->world.scratch=s->scratch;s->world.scratch_count=campaign_movers.count;
    temp=(uint64_t)npc_count*sizeof(*npc_rows);
    if((uint64_t)s->allocated_bytes+temp>budget){status=RF_RANGE;goto fail;}
    if(npc_count){npc_rows=malloc((size_t)temp);if(!npc_rows){status=RF_IO;goto fail;}}
    phase="npc_stage";status=rf_npc_checkpoint_decode(npc->data,npc->bytes,identity,catalog,npc_rows,npc_count,&npc_count);if(status)goto fail;
    s->placement_rows=npc_rows;s->placement_count=npc_count;
    status=scene_npc_checkpoint_restore_prepare_mode(npc_rows,npc_count,catalog,now,scene_world_restore_place,s,
        budget-s->allocated_bytes-(uint32_t)temp,world->fresh_boot,&s->npcs);if(status)goto fail;
    s->allocated_bytes+=s->npcs->bytes;s->placement_rows=NULL;s->placement_count=s->placement_next=0;free(npc_rows);npc_rows=NULL;
    s->semantic_mask=mask;*out=s;return RF_OK;
fail:
    free(mover_rows);free(npc_rows);scene_world_restore_discard(&s);return scene_world_restore_failure(phase,status);
}
/* Read-only real-scene endpoint probe. A successful result proves this subset
 * staged against real resources/geometry; nothing is published or loaded. */
static inline int scene_world_restore_probe(scene_stream *stream,const void *data,uint32_t bytes,
    const unsigned char identity[32],int32_t now)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    scene_world_restore_stage *stage=NULL;scene_checkpoint_world_context world={0};
    rf_npc_checkpoint_catalog catalog={0};rf_geometry_materials mapping={0};rf_geometry_body_surfaces surfaces;
    uint32_t i,allocated=0,mask=0;int status;
    const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");if(!enabled||strcmp(enabled,"1"))return RF_OK;
    if(!stream||stream!=scene_actor_collision_owner||!stream->collision||!actor_follow_world||
       !campaign_surface_sources||!campaign_surface_palette)return scene_world_restore_failure("scene_resources",RF_RANGE);
    mapping.offsets=actor_follow_world->offsets;mapping.slots=actor_follow_world->slots;
    mapping.count=actor_follow_world->geometry_count;mapping.textures.count=actor_follow_world->material_count;
    surfaces.geometries=campaign_surface_sources;surfaces.count=mapping.count;surfaces.mapping=&mapping;surfaces.palette=campaign_surface_palette;
    world.world=stream->collision;world.movers=&campaign_movers;world.metadata=campaign_body_surface;
    world.metadata_context=&surfaces;world.reference_room=0;world.dt=1.0f/60;world.class_speed=0;
    /* This first probe accepts only pristine static geometry. A future loader
     * must prove the same property or stage destruction before giving proof. */
    world.authored_static_unchanged=!stream->terrain&&!stream->terrain_authored&&!stream->terrain_publication&&
        !stream->terrain_history_count&&!stream->terrain_publication_serial;
    catalog.hash=rf_scene_weapon_supply[3];catalog.count=campaign_weapon_supply.names.count;
    if(!catalog.count||catalog.count>64)return scene_world_restore_failure("npc_catalog",RF_RANGE);
    memcpy(catalog.weapons,campaign_weapon_supply.definitions,sizeof(catalog.weapons));
    for(i=0;i<catalog.count;i++){
        const rf_weapon_acquire_definition *d=catalog.weapons+i;
        catalog.supported[i]=(uint8_t)(d->ammo_type>=-1&&d->ammo_type<32&&d->capacity>=0&&d->magazine>=0&&
            (d->ammo_type!=-1||d->magazine==0));
    }
    status=scene_world_restore_prepare(data,bytes,identity,0,&catalog,now,&world,2u*1024u*1024u,&stage);
    if(!status){allocated=stage->allocated_bytes;mask=stage->semantic_mask;}
    printf("WORLD_RESTORE_PROBE status%d bytes%u semantic_mask%u no_publication1\n",status,allocated,mask);
    if(!status)status=scene_world_event_probe(stream,stage,now);
    scene_world_restore_discard(&stage);return status;
#else
    (void)stream;(void)data;(void)bytes;(void)identity;(void)now;return RF_OK;
#endif
}
#endif
