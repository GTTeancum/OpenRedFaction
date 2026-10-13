#ifndef RF_SCENE_CANE_WARMUP_INCLUDED
#define RF_SCENE_CANE_WARMUP_INCLUDED
/* Optional NanoAttackSource only; see CAPEK-CANE-WARMUP.md. Original
 * 42c3fc gates restart on a newly started fire action;421060 advances its
 * independent one-shot clock. No attack/payment/save ownership lives here. */
#define SCENE_CANE_WARMUP_CAPACITY 2u
#define SCENE_CANE_WARMUP_BUDGET (256u*1024u)
#define SCENE_CANE_WARMUP_ALLOWANCE (48u*1024u) /*16KiB stack +32KiB heap/page slack*/
#define SCENE_CANE_WARMUP_CALLBACK 0x488b06u
typedef struct scene_cane_warmup_slot {
    rf_vfx_instance *instances[7];
    uint32_t bound,actor,handle,uid,cls,skeleton,frame,fresh,generation;
    const rf_entity_pose *pose;const float (*matrices)[12];
    uint64_t ticket,queued_ticket;float seconds,transform[12];
} scene_cane_warmup_slot;
typedef struct scene_cane_warmup_owner {
    scene_stream *stream;rf_vfx_mesh *meshes[7];rf_vfx_material_bank *materials;
    rf_vfx_material_textures textures;scene_cane_warmup_slot slots[SCENE_CANE_WARMUP_CAPACITY];
    float direction[3];uint64_t ticket;uint32_t resident_bytes,peak_bytes;
} scene_cane_warmup_owner;
static scene_cane_warmup_owner *scene_cane_warmup;
static struct {scene_stream *stream;rf_vpp *meshes,*maps;uint32_t map_count,attempted;} scene_cane_warmup_pending;
/* status,resident/peak accounted bytes,admission free pages,demand,live,
 * starts,retirements,queue drops,draw polygons,identity/pose skips,pool drops */
uint32_t rf_scene_cane_warmup[12];
static void scene_cane_warmup_slot_reset(scene_cane_warmup_slot *s)
{
    rf_vfx_instance *instances[7];uint32_t i;
    memcpy(instances,s->instances,sizeof(instances));memset(s,0,sizeof(*s));
    memcpy(s->instances,instances,sizeof(instances));
    for(i=0;i<7;i++)if(instances[i])instances[i]->active=0;
}
/* Parent composes this ONLY at final successful world publication after
 * native storage close. Failed load/staging must preserve the outgoing clock. */
static void scene_cane_warmup_reset(void)
{
    uint32_t i;if(scene_cane_warmup)for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++)
        scene_cane_warmup_slot_reset(scene_cane_warmup->slots+i);
    rf_scene_cane_warmup[5]=0;
}
static void scene_cane_warmup_remove(uint32_t handle)
{
    uint32_t i;if(scene_cane_warmup)for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++) {
        scene_cane_warmup_slot *s=scene_cane_warmup->slots+i;
        if(s->bound && s->handle==handle){scene_cane_warmup_slot_reset(s);++rf_scene_cane_warmup[7];}
    }
}
static void scene_cane_warmup_free(scene_cane_warmup_owner *v)
{
    uint32_t i,j;if(!v)return;
    for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++)for(j=0;j<7;j++)rf_vfx_instance_close(v->slots[i].instances+j);
    for(i=0;i<7;i++)rf_vfx_mesh_close(v->meshes+i);
    rf_vfx_material_textures_close(&v->textures);rf_vfx_material_bank_close(&v->materials);free(v);
}
static void scene_cane_warmup_close(void)
{
    scene_cane_warmup_reset();scene_cane_warmup_free(scene_cane_warmup);scene_cane_warmup=NULL;
    memset(&scene_cane_warmup_pending,0,sizeof(scene_cane_warmup_pending));rf_scene_cane_warmup[1]=0;
}
/* Preflight the same first archive as the bitmap loader. Only these exact
 * RGB24 TGA shapes are admitted; decoded allocations are whole native pages. */
static int scene_cane_warmup_image_admit(rf_vpp *maps,uint32_t count,const char *name,uint32_t which)
{
    uint32_t i,size=which==2?128u:64u,bytes=which==2?45645u:12332u;
    int status;unsigned char h[18];rf_vpp_entry entry;
    for(i=0;i<count;i++) {
        status=rf_vpp_find(maps+i,name,&entry);if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(entry.size!=bytes)return RF_FORMAT;
        status=rf_vpp_read(maps+i,&entry,0,h,sizeof(h));if(status)return status;
        if(h[0] || h[1] || (uint32_t)h[2]!=(which==2?10u:2u) ||
           ((uint32_t)h[12]|(uint32_t)h[13]<<8)!=size ||
           ((uint32_t)h[14]|(uint32_t)h[15]<<8)!=size || h[16]!=24 || (h[17]&0xc0))return RF_FORMAT;
        return RF_OK;
    }
    return RF_NOT_FOUND;
}
static int scene_cane_warmup_open(scene_stream *stream,rf_vpp *effects,rf_vpp *maps,uint32_t map_count)
{
    static const uint32_t sizes[14]={2354,2001,2001,2001,2001,2001,2559,211,215,215,215,215,215,214};
    static const char *names[3]={"NanoCaneGlow.tga","Light_Underwater.tga","NanoMuzzleFlash.tga"};
    static const char *mesh_names[7]={"CaneGlow01","CaneLight01","CaneLight02","CaneLight03","CaneLight04","CaneLight05","EnergyMuzzleFlash"};
    rf_vfx_directory directory={0};scene_cane_warmup_owner *v=NULL;unsigned char *raw=NULL;
    uint32_t i,j,used,temporary,peak=0;int status=RF_RANGE;
    uint32_t fixed=(uint32_t)(sizeof(*v)+sizeof(scene_cane_warmup)+sizeof(scene_cane_warmup_pending)+
        sizeof(rf_scene_cane_warmup)+sizeof(sizes)+sizeof(names)+sizeof(mesh_names)+256u+SCENE_CANE_WARMUP_ALLOWANCE);
    if(scene_cane_warmup || !stream || !effects || !maps || !map_count || fixed>=SCENE_CANE_WARMUP_BUDGET)goto done;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_cane_warmup[3]=rf_xbox_available_memory_pages();
    if(rf_scene_cane_warmup[3]<4096u+SCENE_CANE_WARMUP_BUDGET/4096u)goto done;
#endif
    for(i=0;i<3;i++){status=scene_cane_warmup_image_admit(maps,map_count,names[i],i);if(status)goto done;}
    used=fixed;
    status=rf_vfx_directory_open(effects,"NanoAttackSource.vfx",SCENE_CANE_WARMUP_BUDGET-used,&directory);if(status)goto done;
    if(directory.header.version!=0x40006 || directory.entry.size!=16658 || directory.count!=14 ||
       directory.header.values[1]!=37 || directory.header.values[2]!=7 || directory.header.values[9]!=7 ||
       directory.header.values[4] || directory.header.values[5] || directory.header.values[6]){status=RF_FORMAT;goto done;}
    for(i=0;i<14;i++)if(directory.chunks[i].bytes!=sizes[i] ||
        directory.chunks[i].type!=(i<7?0x4f584653u:0x4c54414du)){status=RF_FORMAT;goto done;}
    temporary=directory.allocated_bytes+2559u;
    if((uint64_t)used+temporary>=SCENE_CANE_WARMUP_BUDGET){status=RF_RANGE;goto done;}
    v=calloc(1,sizeof(*v));raw=malloc(2559);if(!v || !raw){status=RF_IO;goto done;}v->stream=stream;
    status=rf_vfx_material_bank_open(&directory,SCENE_CANE_WARMUP_BUDGET-used-temporary,&v->materials);if(status)goto done;
    used+=v->materials->allocated_bytes;if(v->materials->count!=7){status=RF_FORMAT;goto done;}
    for(i=0;i<7;i++) {
        const rf_vfx_material_view *m=v->materials->views+i;float speed,value;uint32_t word;
        memcpy(&speed,m->words+15,4);
        if(m->words[0] || m->words[1] || m->words[2]!=1 || m->words[30]!=15 || m->bitmap_requests!=1 ||
           strcmp((const char *)(m->words+5),names[i==0?0:i==6?2:1]) || m->words[14] || speed!=1 || m->words[16]!=2 ||
           m->words[33] || m->words[34] || m->words[35] || m->words[46]!=1 || m->words[48]!=37){status=RF_FORMAT;goto done;}
        status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,1,0,&word);if(status)goto done;
        memcpy(&value,&word,4);if(value!=1){status=RF_FORMAT;goto done;}
        for(j=0;j<37;j++) {
            status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,2,j,&word);if(status)goto done;
            memcpy(&value,&word,4);if(!isfinite(value) || value<0 || value>1){status=RF_FORMAT;goto done;}
        }
        status=rf_vfx_chunk_read(&directory,i,0,raw,sizes[i]);if(status)goto done;
        status=rf_vfx_mesh_open(raw,sizes[i],0x40006,7,NULL,SCENE_CANE_WARMUP_BUDGET-used-temporary,v->meshes+i);if(status)goto done;
        used+=v->meshes[i]->allocated_bytes;
        {const rf_vfx_mesh *m=v->meshes[i];
         if(strcmp(m->prefix.name,mesh_names[i]) || strcmp(m->prefix.parent,i?"CaneGlow01":"muzzle_1") ||
            m->prefix.vertices!=4 || m->prefix.faces!=(i?2u:4u) || m->prefix.timing.samples!=38 ||
            m->prefix.timing.flags!=60 || m->prefix.timing.start!=0 || m->prefix.timing.end!=2.4666666984558105f ||
            m->materials!=1 || scene_cane_word(m->data+m->material_offset)!=i ||
            m->edges.flags!=(i==0?0x810u:i==6?0x15u:0x11u) || m->edges.mesh_flags!=60){status=RF_FORMAT;goto done;}}
    }
    {double length=0;const float *d=v->meshes[0]->frames[0].direction;
     if(!(v->meshes[0]->frames[0].present&4)){status=RF_FORMAT;goto done;}
     for(i=0;i<3;i++)length+=(double)d[i]*d[i];
     if(!isfinite(length) || length<=0){status=RF_FORMAT;goto done;}length=sqrt(length);
     for(i=0;i<3;i++)v->direction[i]=(float)(d[i]/length);}
    peak=used+temporary;free(raw);raw=NULL;rf_vfx_directory_close(&directory);temporary=0;
    for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++)for(j=0;j<7;j++) {
        status=rf_vfx_instance_open(v->meshes[j],SCENE_CANE_WARMUP_BUDGET-used,v->slots[i].instances+j);if(status)goto done;
        used+=v->slots[i].instances[j]->allocated_bytes;
    }
    status=rf_vfx_material_textures_open(&v->textures,v->materials->views,7,maps,map_count,
        SCENE_CANE_WARMUP_BUDGET-used+(uint32_t)sizeof(v->textures));if(status)goto done;
    if(v->textures.resident_bytes<sizeof(v->textures) || !v->textures.bindings || !v->textures.textures ||
       v->textures.count!=7 || v->textures.texture_count!=3){status=RF_FORMAT;goto done;}
    used+=v->textures.resident_bytes-(uint32_t)sizeof(v->textures);
    for(i=0;i<7;i++)if(v->textures.bindings[i][0]!=(i==0?0u:i==6?2u:1u) ||
        v->textures.bindings[i][1]!=UINT32_MAX || v->textures.bindings[i][2]!=UINT32_MAX){status=RF_FORMAT;goto done;}
    for(i=0;i<3;i++) {
        const rf_particle_animation *a=&v->textures.textures[i].animation;uint32_t size=i==2?128u:64u;
        if(strcmp(v->textures.textures[i].name,names[i]) || a->count!=1 || !a->images || !a->images[0].rgba ||
           a->images[0].width!=size || a->images[0].height!=size || a->images[0].source_format!=6 ||
           a->images[0].bytes!=size*size*4){status=RF_FORMAT;goto done;}
    }
    if(used>peak)peak=used;if(peak>SCENE_CANE_WARMUP_BUDGET){status=RF_RANGE;goto done;}
    v->resident_bytes=used;v->peak_bytes=peak;rf_scene_cane_warmup[1]=used;rf_scene_cane_warmup[2]=peak;
    scene_cane_warmup=v;v=NULL;status=RF_OK;
done:
    free(raw);rf_vfx_directory_close(&directory);scene_cane_warmup_free(v);
    rf_scene_cane_warmup[0]=(uint32_t)status;return status;
}
static int scene_cane_warmup_actor(uint32_t actor,rf_entity_pose **out)
{
    campaign_npc_body *b;uint32_t cls;rf_entity_pose *p=NULL;
    if(!campaign_npc_bodies || !campaign_model_owners || !campaign_seeds.items || !campaign_seeds.classes ||
       !campaign_seeds.records.items || !campaign_skeletons.items || !campaign_skeletons.class_indices ||
       !campaign_render_models.items || actor>=campaign_npc_body_count ||
       actor>=campaign_seeds.records.count || actor>=campaign_model_owner_count)return 0;
    b=campaign_npc_bodies+actor;cls=campaign_seeds.items[actor].class_index;
    if(cls>=campaign_seeds.class_count || strcmp(campaign_seeds.records.items[actor].record.class_name,"Capek") ||
       b->registration.view!=&b->view || rf_object_registry_lookup(&campaign_registry,b->registration.handle)!=&b->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)b->registration.handle)!=&b->view ||
       (b->object_flags&2u) || (b->view.flags_810&1u) || !isfinite(b->damage.effects.health) || b->damage.effects.health<=0 ||
       campaign_actor_pose(actor,&p) || !p || !p->matrices || !p->generations ||
       p->skeleton>=campaign_skeletons.count || p->skeleton>=campaign_render_models.count ||
       cls>=campaign_skeletons.class_count || campaign_skeletons.class_indices[cls]!=p->skeleton ||
       !campaign_skeletons.items[p->skeleton].bones || campaign_skeletons.items[p->skeleton].count!=p->bone_count ||
       campaign_render_models.items[p->skeleton].bone_count!=p->bone_count || !p->bone_count || p->bone_count>50)return 0;
    *out=p;return 1;
}
static int scene_cane_warmup_matches(const scene_cane_warmup_slot *s,rf_entity_pose **out)
{
    rf_entity_pose *p;if(!s->bound || !scene_cane_warmup_actor(s->actor,&p))return 0;
    if(s->handle!=campaign_npc_bodies[s->actor].registration.handle ||
       s->uid!=(uint32_t)campaign_seeds.records.items[s->actor].record.uid ||
       s->cls!=campaign_seeds.items[s->actor].class_index || s->skeleton!=p->skeleton || s->pose!=p || s->matrices!=p->matrices)return 0;
    *out=p;return 1;
}
/* Called solely after a previously inactive action2 has actually started.
 * No allocation, muzzle fallback, shot receipt or per-tick restart. */
static void scene_cane_warmup_start(uint32_t actor,uint32_t handle,const rf_entity_pose *accepted_pose)
{
    scene_cane_warmup_owner *v=scene_cane_warmup;scene_cane_warmup_slot *s=NULL;rf_entity_pose *p;uint32_t i;
    if(!v || !scene_cane_warmup_actor(actor,&p) || p!=accepted_pose ||
       campaign_npc_bodies[actor].registration.handle!=handle ||
       !campaign_enemy_cane_weapon(campaign_npc_bodies[actor].view.weapons[0]) || v->ticket==UINT64_MAX)return;
    for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++)if(v->slots[i].bound && v->slots[i].actor==actor){s=v->slots+i;break;}
    if(!s)for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++)if(!v->slots[i].bound){s=v->slots+i;break;}
    if(!s){++rf_scene_cane_warmup[11];return;}
    scene_cane_warmup_slot_reset(s);s->bound=s->fresh=1;s->actor=actor;s->handle=handle;
    s->uid=(uint32_t)campaign_seeds.records.items[actor].record.uid;s->cls=campaign_seeds.items[actor].class_index;
    s->skeleton=p->skeleton;s->pose=p;s->matrices=p->matrices;s->ticket=++v->ticket;++rf_scene_cane_warmup[6];
}
static void scene_cane_warmup_tick(scene_stream *stream,float dt,uint32_t frame)
{
    scene_cane_warmup_owner *v=scene_cane_warmup;uint32_t i;rf_entity_pose *p;
    if(!v || v->stream!=stream || !isfinite(dt) || dt<0)return;
    rf_scene_cane_warmup[5]=0;
    for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++) {
        scene_cane_warmup_slot *s=v->slots+i;if(!s->bound)continue;
        if(!scene_cane_warmup_matches(s,&p)){scene_cane_warmup_slot_reset(s);++rf_scene_cane_warmup[7];continue;}
        if(s->fresh){s->fresh=0;s->frame=frame;}
        else if(s->frame!=frame){s->frame=frame;s->seconds=(float)((double)s->seconds+dt);}
        s->queued_ticket=0;
        if(!isfinite(s->seconds) || s->seconds*15.f>=37.f){scene_cane_warmup_slot_reset(s);++rf_scene_cane_warmup[7];continue;}
        ++rf_scene_cane_warmup[5];
    }
}
static void scene_cane_warmup_prepare(scene_stream *stream,rf_vpp *meshes,rf_vpp *maps,uint32_t map_count)
{
    scene_cane_warmup_pending.stream=stream;scene_cane_warmup_pending.meshes=meshes;
    scene_cane_warmup_pending.maps=maps;scene_cane_warmup_pending.map_count=map_count;
    scene_cane_warmup_pending.attempted=0;memset(rf_scene_cane_warmup,0,sizeof(rf_scene_cane_warmup));
}
static void scene_cane_warmup_admit(scene_stream *stream)
{
    uint32_t i,demand=0;
    if(scene_cane_warmup_pending.stream!=stream || scene_cane_warmup_pending.attempted)return;
    scene_cane_warmup_pending.attempted=1;
    if(!campaign_seeds.records.items)return;
    for(i=0;i<campaign_seeds.records.count;i++)if(!strcmp(campaign_seeds.records.items[i].record.class_name,"Capek"))++demand;
    rf_scene_cane_warmup[4]=demand;if(!demand)return;
    if(demand>SCENE_CANE_WARMUP_CAPACITY){rf_scene_cane_warmup[0]=RF_RANGE;return;}
    (void)scene_cane_warmup_open(stream,scene_cane_warmup_pending.meshes,scene_cane_warmup_pending.maps,scene_cane_warmup_pending.map_count);
}
static void scene_cane_warmup_transform(const float local[3],const float transform[12],float out[3])
{
    uint32_t i;for(i=0;i<3;i++)out[i]=(float)((double)transform[9+i]+(double)transform[i]*local[0]+
        (double)transform[3+i]*local[1]+(double)transform[6+i]*local[2]);
}
static void scene_cane_warmup_queue_room(scene_stream *stream,uint32_t room,rf_render_queue_record *records,uint32_t capacity,uint32_t *count)
{
    scene_cane_warmup_owner *v=scene_cane_warmup;uint32_t i,j,accepted;int status;
    if(!v || v->stream!=stream)return;
    for(i=0;i<SCENE_CANE_WARMUP_CAPACITY;i++) {
        scene_cane_warmup_slot *s=v->slots+i;rf_entity_pose *p;const campaign_model_owner *model;
        rf_render_queue_record entry={0};rf_corpse_surface_source source={0};int32_t tag;
        if(!scene_cane_warmup_matches(s,&p)){s->queued_ticket=0;continue;}
        model=campaign_model_owners+s->actor;
        if(model->room+1!=room || (campaign_npc_bodies[s->actor].object_flags&(2u|0x4000u)))continue;
        s->queued_ticket=0;if(p->playback.generation>65535){++rf_scene_cane_warmup[10];continue;}
        for(j=0;j<p->bone_count;j++)if(p->generations[j]!=(uint16_t)p->playback.generation)break;
        if(j!=p->bone_count){++rf_scene_cane_warmup[10];continue;}
        status=rf_scene_corpse_file_tag(s->actor+1,"muzzle_1",0,&tag);
        if(status || tag<0){++rf_scene_cane_warmup[10];continue;}
        source.model=s->actor+1;memcpy(source.position,model->position,12);memcpy(source.basis,model->basis,36);
        status=campaign_file_tag_pose(&source,tag,s->transform);if(status){++rf_scene_cane_warmup[10];continue;}
        for(j=0;j<12;j++)if(!isfinite(s->transform[j]))break;if(j!=12)continue;
        s->generation=p->playback.generation;
        /* Maximum installed flash radius2.107 plus authored offsets<1.
         * A conservative3m sphere avoids clipping the beam/glow at the muzzle. */
        memcpy(entry.position,s->transform+9,12);entry.radius=3;
        entry.object=i;entry.sorted=1;entry.lighting_flag=1;entry.callback=SCENE_CANE_WARMUP_CALLBACK;
        status=rf_render_queue_append(&stream->particle_camera.frustum,entry.position,&entry,records,capacity,count,&accepted);
        if(status || !accepted){++rf_scene_cane_warmup[8];continue;}s->queued_ticket=s->ticket;
    }
}
static int scene_cane_warmup_draw(scene_stream *stream,uint32_t slot,rf_scene_particle_sink sink,void *context)
{
    scene_cane_warmup_owner *v=scene_cane_warmup;scene_cane_warmup_slot *s;rf_entity_pose *p;
    uint32_t i,j;int status;float time;
    if(!v || v->stream!=stream || slot>=SCENE_CANE_WARMUP_CAPACITY)return RF_OK;s=v->slots+slot;
    if(!s->queued_ticket || s->queued_ticket!=s->ticket || !scene_cane_warmup_matches(s,&p) ||
       p->playback.generation!=s->generation || (campaign_npc_bodies[s->actor].object_flags&(2u|0x4000u)))return RF_OK;
    for(i=0;i<p->bone_count;i++)if(p->generations[i]!=(uint16_t)s->generation)return RF_OK;
    time=s->seconds*15.f;
    for(i=0;i<7;i++) {
        rf_vfx_instance *instance=s->instances[i];const rf_image *image;
        rf_particle_screen_polygon polygon={0};rf_particle_draw_vertex vertices[12];rf_particle_vertex_environment env={0};
        rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};float opacity,center[3];uint32_t rgba;
        status=rf_vfx_instance_update(instance,time);if(status)return status;if(!instance->active)continue;
        if(instance->extra[0]<=0 || instance->extra[1]<=0)continue;
        status=rf_vfx_material_evaluate(v->materials->data,v->materials->bytes,v->materials->views+i,2,time,&opacity);if(status)return status;
        rgba=0x00ffffffu|((uint32_t)(fmaxf(0,fminf(1,opacity))*255.f+.5f)<<24);if(!(rgba>>24))continue;
        status=rf_vfx_material_texture_sample(&v->textures,v->materials->views+i,i,0,time,0,&image);if(status)return status;
        /* Parent strings are not transform bindings in this original path.
         *53cde0 clears parent flags;421fd3 supplies actor muzzle to all meshes.
         * Never compose CaneGlow01 again or substitute the held weapon muzzle. */
        if(!i) {
            float minus[3],plus[3],first[3],second[3];
            for(j=0;j<3;j++){float half=(float)((double)v->direction[j]*instance->extra[1]*.5);
                minus[j]=instance->center[j]-half;plus[j]=instance->center[j]+half;}
            scene_cane_warmup_transform(minus,s->transform,first);scene_cane_warmup_transform(plus,s->transform,second);
            status=rf_volume_beam_project(&stream->particle_camera,second,first,instance->extra[0],&polygon);
        }else {
            scene_cane_warmup_transform(instance->center,s->transform,center);
            status=scene_cane_flare_project(&stream->particle_camera,center,instance->extra,&polygon);
        }
        if(status)return status;if(!polygon.count)continue;if(polygon.count>12)return RF_RANGE;
        status=rf_particle_render_decode(RF_PARTICLE_GLOW_MODE,&re,&states);if(status)return status;
        env.rgba=rgba;env.vertex_color=states.vertex_color;env.vertex_alpha=states.vertex_alpha;
        env.depth_scale=env.reciprocal_scale=stream->particle_camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
        for(j=0;j<polygon.count;j++){status=rf_particle_vertex_encode(&env,polygon.vertices+j,vertices+j);if(status)return status;}
        if(sink){status=sink(context,vertices,polygon.count,image,RF_PARTICLE_GLOW_MODE);if(status)return status;}
        ++rf_scene_cane_warmup[9];
    }
    return RF_OK;
}
#endif
