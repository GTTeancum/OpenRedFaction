#ifndef RF_SCENE_NANO_VISUAL_INCLUDED
#define RF_SCENE_NANO_VISUAL_INCLUDED
/* Constant shield only: RF.exe422ad0/4210c0/4222d4, documented in
 * NANO-SHIELD-CONSTANT.md. No damage, event, sound or serialized ownership. */
#define SCENE_NANO_VISUAL_CAPACITY 2u
#define SCENE_NANO_VISUAL_BUDGET (1024u*1024u)
#define SCENE_NANO_VISUAL_RESERVE (80u*1024u) /*16KiB stack +64KiB heap/page slack*/
#define SCENE_NANO_DRAW_CALLBACK 0x488b05u
typedef struct scene_nano_visual_slot {
    rf_vfx_instance *instances[2];
    uint32_t bound,actor,handle,uid,cls,skeleton,frame,queued,generation;
    const rf_entity_pose *pose;const float (*matrices)[12];
    float seconds,position[3],basis[9];
} scene_nano_visual_slot;
typedef struct scene_nano_visual_owner {
    scene_stream *stream;rf_vfx_mesh *meshes[2];rf_vfx_material_bank *materials;
    rf_vfx_material_textures textures;scene_nano_visual_slot slots[SCENE_NANO_VISUAL_CAPACITY];
    uint32_t resident_bytes,peak_bytes;
} scene_nano_visual_owner;
static scene_nano_visual_owner *scene_nano_visual;
static struct {scene_stream *stream;rf_vpp *meshes,*maps;uint32_t map_count,attempted;} scene_nano_visual_pending;
/* status,resident/peak accounted bytes,admission free pages,demand,live,
 * bindings,retirements,queue drops,draw polygons,identity/pose skips */
uint32_t rf_scene_nano_visual[11];
static void scene_nano_visual_slot_reset(scene_nano_visual_slot *s)
{
    rf_vfx_instance *a=s->instances[0],*b=s->instances[1];
    memset(s,0,sizeof(*s));s->instances[0]=a;s->instances[1]=b;
    if(a)a->active=0;if(b)b->active=0;
}
/* Parent calls only from final successful world publication, after storage
 * close. Failed loads must not discard the outgoing visual timeline. */
static void scene_nano_visual_reset(void)
{
    uint32_t i;if(scene_nano_visual)for(i=0;i<SCENE_NANO_VISUAL_CAPACITY;i++)
        scene_nano_visual_slot_reset(scene_nano_visual->slots+i);
    rf_scene_nano_visual[5]=0;
}
static void scene_nano_visual_remove(uint32_t handle)
{
    uint32_t i;if(scene_nano_visual)for(i=0;i<SCENE_NANO_VISUAL_CAPACITY;i++) {
        scene_nano_visual_slot *s=scene_nano_visual->slots+i;
        if(s->bound && s->handle==handle){scene_nano_visual_slot_reset(s);++rf_scene_nano_visual[7];}
    }
}
static void scene_nano_visual_free(scene_nano_visual_owner *v)
{
    uint32_t i,j;if(!v)return;
    for(i=0;i<SCENE_NANO_VISUAL_CAPACITY;i++) {
        scene_nano_visual_slot_reset(v->slots+i);
        for(j=0;j<2;j++)rf_vfx_instance_close(v->slots[i].instances+j);
    }
    for(i=0;i<2;i++)rf_vfx_mesh_close(v->meshes+i);
    rf_vfx_material_textures_close(&v->textures);rf_vfx_material_bank_close(&v->materials);free(v);
}
static void scene_nano_visual_close(void)
{
    scene_nano_visual_reset();scene_nano_visual_free(scene_nano_visual);scene_nano_visual=NULL;
    memset(&scene_nano_visual_pending,0,sizeof(scene_nano_visual_pending));rf_scene_nano_visual[1]=0;
}
/* Match the first archive chosen by the shared bitmap reader before allowing
 * any pixel allocation. Thus modified huge/VBM inputs cannot hide page padding. */
static int scene_nano_visual_image_admit(rf_vpp *maps,uint32_t count,const char *name,uint32_t size)
{
    uint32_t i;int status;unsigned char h[18];rf_vpp_entry entry;
    for(i=0;i<count;i++) {
        status=rf_vpp_find(maps+i,name,&entry);if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(entry.size!=size*size*4+44)return RF_FORMAT;
        status=rf_vpp_read(maps+i,&entry,0,h,sizeof(h));if(status)return status;
        if(h[0] || h[1] || h[2]!=2 || ((uint32_t)h[12]|((uint32_t)h[13]<<8u))!=size ||
           ((uint32_t)h[14]|((uint32_t)h[15]<<8u))!=size || h[16]!=32 || (h[17]&0xc0))return RF_FORMAT;
        return RF_OK;
    }
    return RF_NOT_FOUND;
}
static int scene_nano_visual_open(scene_stream *stream,rf_vpp *effects,rf_vpp *maps,uint32_t map_count)
{
    static const uint32_t sizes[4]={649,408718,195,191};
    static const char *names[2]={"NanoShieldGlow01.tga","NanoShield01.tga"};
    static const char *mesh_names[2]={"Plane01","Sphere01"};
    rf_vfx_directory directory={0};scene_nano_visual_owner *v=NULL;unsigned char *raw=NULL;
    uint32_t i,j,used,temporary,peak=0;int status=RF_RANGE;
    uint32_t fixed=(uint32_t)(sizeof(*v)+sizeof(scene_nano_visual)+sizeof(scene_nano_visual_pending)+
        sizeof(rf_scene_nano_visual)+SCENE_NANO_VISUAL_RESERVE);
    if(scene_nano_visual || !stream || !effects || !maps || !map_count || fixed>=SCENE_NANO_VISUAL_BUDGET)goto done;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_nano_visual[3]=rf_xbox_available_memory_pages();
    if(rf_scene_nano_visual[3]<4096u+SCENE_NANO_VISUAL_BUDGET/4096u)goto done;
#endif
    for(i=0;i<2;i++){status=scene_nano_visual_image_admit(maps,map_count,names[i],i?128:64);if(status)goto done;}
    used=fixed;
    status=rf_vfx_directory_open(effects,"NanoShieldConstant.vfx",SCENE_NANO_VISUAL_BUDGET-used,&directory);if(status)goto done;
    if(directory.header.version!=0x40006 || directory.entry.size!=409913 || directory.count!=4 ||
       directory.header.values[1]!=32 || directory.header.values[2]!=2 || directory.header.values[9]!=2 ||
       directory.header.values[4] || directory.header.values[5] || directory.header.values[6]){status=RF_FORMAT;goto done;}
    for(i=0;i<4;i++)if(directory.chunks[i].bytes!=sizes[i] ||
        directory.chunks[i].type!=(i<2?0x4f584653u:0x4c54414du)){status=RF_FORMAT;goto done;}
    temporary=directory.allocated_bytes+408718u;
    if((uint64_t)used+temporary>=SCENE_NANO_VISUAL_BUDGET){status=RF_RANGE;goto done;}
    v=calloc(1,sizeof(*v));raw=malloc(408718);if(!v || !raw){status=RF_IO;goto done;}
    v->stream=stream;
    status=rf_vfx_material_bank_open(&directory,SCENE_NANO_VISUAL_BUDGET-used-temporary,&v->materials);if(status)goto done;
    used+=v->materials->allocated_bytes;
    if(v->materials->count!=2){status=RF_FORMAT;goto done;}
    for(i=0;i<2;i++) {
        const rf_vfx_material_view *m=v->materials->views+i;float speed,value;uint32_t word;
        memcpy(&speed,m->words+15,4);
        if(m->words[0] || m->words[1] || m->words[2]!=1 || m->words[30]!=15 || m->bitmap_requests!=1 ||
           strcmp((const char *)(m->words+5),names[i]) || m->words[14] || speed!=1 || m->words[16]!=2 ||
           m->words[33] || m->words[34] || m->words[35] || m->words[46]!=1 || m->words[48]!=32){status=RF_FORMAT;goto done;}
        status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,1,0,&word);if(status)goto done;
        memcpy(&value,&word,4);if(value!=1){status=RF_FORMAT;goto done;}
        for(j=0;j<32;j++) {
            status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,2,j,&word);if(status)goto done;
            memcpy(&value,&word,4);if(!isfinite(value) || value<0 || value>1){status=RF_FORMAT;goto done;}
        }
        status=rf_vfx_chunk_read(&directory,i,0,raw,sizes[i]);if(status)goto done;
        status=rf_vfx_mesh_open(raw,sizes[i],0x40006,2,NULL,SCENE_NANO_VISUAL_BUDGET-used-temporary,v->meshes+i);if(status)goto done;
        used+=v->meshes[i]->allocated_bytes;
        {const rf_vfx_mesh *m=v->meshes[i];
         if(strcmp(m->prefix.name,mesh_names[i]) || strcmp(m->prefix.parent,"Scene Root") ||
            m->prefix.vertices!=(i?114u:4u) || m->prefix.faces!=(i?448u:2u) || m->prefix.timing.samples!=33 ||
            m->prefix.timing.flags!=60 || m->prefix.timing.start!=0 || m->prefix.timing.end!=2.1666667461395264f ||
            m->materials!=1 || scene_cane_word(m->data+m->material_offset)!=i ||
            m->edges.flags!=(i?0x110u:0x11u) || m->edges.mesh_flags!=62){status=RF_FORMAT;goto done;}}
    }
    peak=used+temporary; /* Full retained Sphere payload overlaps its raw input. */
    free(raw);raw=NULL;rf_vfx_directory_close(&directory);temporary=0;
    for(i=0;i<SCENE_NANO_VISUAL_CAPACITY;i++)for(j=0;j<2;j++) {
        status=rf_vfx_instance_open(v->meshes[j],SCENE_NANO_VISUAL_BUDGET-used,v->slots[i].instances+j);if(status)goto done;
        used+=v->slots[i].instances[j]->allocated_bytes;
    }
    status=rf_vfx_material_textures_open(&v->textures,v->materials->views,2,maps,map_count,
        SCENE_NANO_VISUAL_BUDGET-used+(uint32_t)sizeof(v->textures));if(status)goto done;
    if(v->textures.resident_bytes<sizeof(v->textures) || !v->textures.bindings || !v->textures.textures ||
       v->textures.count!=2 || v->textures.texture_count!=2){status=RF_FORMAT;goto done;}
    used+=v->textures.resident_bytes-(uint32_t)sizeof(v->textures);
    for(i=0;i<2;i++) {
        const rf_particle_animation *a=&v->textures.textures[i].animation;uint32_t size=i?128:64;
        if(v->textures.bindings[i][0]!=i || v->textures.bindings[i][1]!=UINT32_MAX || v->textures.bindings[i][2]!=UINT32_MAX ||
           strcmp(v->textures.textures[i].name,names[i]) || a->count!=1 || !a->images || !a->images[0].rgba ||
           a->images[0].width!=size || a->images[0].height!=size || a->images[0].source_format!=7 ||
           a->images[0].bytes!=size*size*4){status=RF_FORMAT;goto done;}
        /* Both exact native allocations are whole pages; padding is zero. */
    }
    if(used>peak)peak=used;if(peak>SCENE_NANO_VISUAL_BUDGET){status=RF_RANGE;goto done;}
    v->resident_bytes=used;v->peak_bytes=peak;rf_scene_nano_visual[1]=used;rf_scene_nano_visual[2]=peak;
    scene_nano_visual=v;v=NULL;status=RF_OK;
done:
    free(raw);rf_vfx_directory_close(&directory);scene_nano_visual_free(v);
    rf_scene_nano_visual[0]=(uint32_t)status;return status;
}
/* No allocation/evaluation here: identity includes full registry generation,
 * stable authored slot/UID/class, current model and current pose storage. */
static int scene_nano_visual_actor(uint32_t actor,rf_entity_pose **out)
{
    campaign_npc_body *b;uint32_t cls;rf_entity_pose *p=NULL;
    if(!campaign_npc_bodies || !campaign_model_owners || !campaign_seeds.items || !campaign_seeds.classes ||
       !campaign_seeds.records.items || !campaign_skeletons.items || !campaign_skeletons.class_indices ||
       !campaign_render_models.items || actor>=campaign_npc_body_count ||
       actor>=campaign_seeds.records.count || actor>=campaign_model_owner_count)return 0;
    b=campaign_npc_bodies+actor;cls=campaign_seeds.items[actor].class_index;
    if(cls>=campaign_seeds.class_count || !(campaign_seeds.classes[cls].physics.flags&0x02000000u) ||
       b->registration.view!=&b->view || rf_object_registry_lookup(&campaign_registry,b->registration.handle)!=&b->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)b->registration.handle)!=&b->view ||
       campaign_actor_pose(actor,&p) || !p || !p->matrices || !p->generations ||
       p->skeleton>=campaign_skeletons.count || p->skeleton>=campaign_render_models.count ||
       cls>=campaign_skeletons.class_count || campaign_skeletons.class_indices[cls]!=p->skeleton ||
       !campaign_skeletons.items[p->skeleton].bones || campaign_skeletons.items[p->skeleton].count!=p->bone_count ||
       campaign_render_models.items[p->skeleton].bone_count!=p->bone_count || !p->bone_count || p->bone_count>50)return 0;
    *out=p;return 1;
}
static int scene_nano_visual_matches(const scene_nano_visual_slot *s,rf_entity_pose **out)
{
    rf_entity_pose *p;if(!s->bound || !scene_nano_visual_actor(s->actor,&p))return 0;
    if(s->handle!=campaign_npc_bodies[s->actor].registration.handle ||
       s->uid!=(uint32_t)campaign_seeds.records.items[s->actor].record.uid ||
       s->cls!=campaign_seeds.items[s->actor].class_index || s->skeleton!=p->skeleton || s->pose!=p || s->matrices!=p->matrices)return 0;
    *out=p;return 1;
}
static int scene_nano_visual_enabled(const scene_nano_visual_slot *s)
{
    const campaign_npc_body *b=campaign_npc_bodies+s->actor;
    return isfinite(b->damage.effects.armor) && rf_entity_armor_immunity(b->damage.effects.armor,
        campaign_seeds.classes[s->cls].physics.flags,b->damage.effects.flags_814);
}
static void scene_nano_visual_tick(scene_stream *stream,float dt,uint32_t frame)
{
    scene_nano_visual_owner *v=scene_nano_visual;uint32_t i,j;rf_entity_pose *p;
    if(!v || v->stream!=stream || !isfinite(dt) || dt<0)return;
    for(i=0;i<SCENE_NANO_VISUAL_CAPACITY;i++)if(v->slots[i].bound && !scene_nano_visual_matches(v->slots+i,&p)) {
        scene_nano_visual_slot_reset(v->slots+i);++rf_scene_nano_visual[7];
    }
    for(i=0;i<campaign_npc_body_count;i++)if(scene_nano_visual_actor(i,&p)) {
        scene_nano_visual_slot *s=NULL;
        for(j=0;j<SCENE_NANO_VISUAL_CAPACITY;j++)if(v->slots[j].bound && v->slots[j].actor==i){s=v->slots+j;break;}
        if(!s)for(j=0;j<SCENE_NANO_VISUAL_CAPACITY;j++)if(!v->slots[j].bound) {
            s=v->slots+j;s->bound=1;s->actor=i;s->handle=campaign_npc_bodies[i].registration.handle;
            s->uid=(uint32_t)campaign_seeds.records.items[i].record.uid;s->cls=campaign_seeds.items[i].class_index;
            s->skeleton=p->skeleton;s->pose=p;s->matrices=p->matrices;s->frame=frame-1;++rf_scene_nano_visual[6];break;
        }
        if(!s || s->frame==frame)continue;s->frame=frame;s->queued=0;
        if(scene_nano_visual_enabled(s)) {
            /*54ccf8..54cd78: stored seconds,15Hz,32-frame looping header.
             * fmod bounds arbitrary elapsed input without unbounded subtraction. */
            double seconds=(double)s->seconds+dt;const float duration=32.f*.06666667014360428f;
            s->seconds=(float)fmod(seconds,duration);
            if(s->seconds*15.f>=32.f)s->seconds=0;
        }
    }
    rf_scene_nano_visual[5]=0;
    for(i=0;i<SCENE_NANO_VISUAL_CAPACITY;i++)rf_scene_nano_visual[5]+=v->slots[i].bound;
}
static void scene_nano_visual_prepare(scene_stream *stream,rf_vpp *meshes,rf_vpp *maps,uint32_t map_count)
{
    scene_nano_visual_pending.stream=stream;scene_nano_visual_pending.meshes=meshes;
    scene_nano_visual_pending.maps=maps;scene_nano_visual_pending.map_count=map_count;
    scene_nano_visual_pending.attempted=0;memset(rf_scene_nano_visual,0,sizeof(rf_scene_nano_visual));
}
/* First frame after core animation/session/load allocations have completed.
 * Archive pointers are borrowed only until the enclosing scene call returns. */
static void scene_nano_visual_admit(scene_stream *stream,uint32_t frame)
{
    uint32_t i,demand=0;
    if(scene_nano_visual_pending.stream!=stream || scene_nano_visual_pending.attempted)return;
    scene_nano_visual_pending.attempted=1;
    for(i=0;i<campaign_seeds.records.count;i++)if(campaign_seeds.items[i].class_index<campaign_seeds.class_count &&
        (campaign_seeds.classes[campaign_seeds.items[i].class_index].physics.flags&0x02000000u))++demand;
    rf_scene_nano_visual[4]=demand;if(!demand)return;
    if(demand>SCENE_NANO_VISUAL_CAPACITY){rf_scene_nano_visual[0]=RF_RANGE;return;}
    (void)scene_nano_visual_open(stream,scene_nano_visual_pending.meshes,scene_nano_visual_pending.maps,scene_nano_visual_pending.map_count);
    scene_nano_visual_tick(stream,0,frame);
}
static void scene_nano_visual_transform(const float p[3],const float origin[3],const float basis[9],float out[3])
{
    uint32_t i;for(i=0;i<3;i++)out[i]=(float)((double)origin[i]+(double)basis[i]*p[0]+(double)basis[3+i]*p[1]+(double)basis[6+i]*p[2]);
}
static void scene_nano_visual_queue_room(scene_stream *stream,uint32_t room,rf_render_queue_record *records,uint32_t capacity,uint32_t *count)
{
    scene_nano_visual_owner *v=scene_nano_visual;uint32_t i,j,k,accepted;int status;
    if(!v || v->stream!=stream)return;
    for(i=0;i<SCENE_NANO_VISUAL_CAPACITY;i++) {
        scene_nano_visual_slot *s=v->slots+i;rf_entity_pose *p;const campaign_model_owner *model;
        rf_render_queue_record entry={0};float root[3]={0};
        if(!scene_nano_visual_matches(s,&p)) {s->queued=0;continue;}
        model=campaign_model_owners+s->actor;
        if(model->room+1!=room || !scene_nano_visual_enabled(s) || (campaign_npc_bodies[s->actor].object_flags&(2u|0x4000u)))continue;
        s->queued=0;if(p->playback.generation>65535){++rf_scene_nano_visual[10];continue;}
        for(j=0;j<p->bone_count;j++)if(p->generations[j]!=(uint16_t)p->playback.generation)break;
        if(j!=p->bone_count){++rf_scene_nano_visual[10];continue;}
        /*48ac70/5011f0: first negative-parent bone, otherwise actor origin. */
        for(j=0;j<p->bone_count;j++)if(campaign_skeletons.items[p->skeleton].bones[j].parent<0){memcpy(root,p->matrices[j]+9,12);break;}
        memcpy(s->basis,model->basis,36);scene_nano_visual_transform(root,model->position,s->basis,s->position);
        for(k=0;k<3;k++)if(!isfinite(s->position[k]))break;if(k!=3)continue;
        s->generation=p->playback.generation;
        memcpy(entry.position,s->position,12);entry.radius=fmaxf(v->meshes[0]->edges.radius,v->meshes[1]->edges.radius);
        entry.object=i;entry.sorted=1;entry.lighting_flag=1;entry.callback=SCENE_NANO_DRAW_CALLBACK;
        status=rf_render_queue_append(&stream->particle_camera.frustum,entry.position,&entry,records,capacity,count,&accepted);
        if(status || !accepted){++rf_scene_nano_visual[8];continue;}s->queued=1;
    }
}
static int scene_nano_visual_submit(scene_stream *stream,const rf_particle_screen_polygon *polygon,
    const rf_image *image,uint32_t rgba,rf_scene_particle_sink sink,void *context)
{
    rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    rf_particle_vertex_environment env={0};rf_particle_draw_vertex vertices[12];uint32_t i;int status;
    if(!polygon->count || !(rgba>>24))return RF_OK;if(polygon->count>12)return RF_RANGE;
    status=rf_particle_render_decode(RF_PARTICLE_GLOW_MODE,&re,&states);if(status)return status;
    env.rgba=rgba;env.vertex_color=states.vertex_color;env.vertex_alpha=states.vertex_alpha;
    env.depth_scale=env.reciprocal_scale=stream->particle_camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
    for(i=0;i<polygon->count;i++){status=rf_particle_vertex_encode(&env,polygon->vertices+i,vertices+i);if(status)return status;}
    if(sink){status=sink(context,vertices,polygon->count,image,RF_PARTICLE_GLOW_MODE);if(status)return status;}
    ++rf_scene_nano_visual[9];return RF_OK;
}
static int scene_nano_visual_draw(scene_stream *stream,uint32_t slot,rf_scene_particle_sink sink,void *context)
{
    scene_nano_visual_owner *v=scene_nano_visual;scene_nano_visual_slot *s;rf_entity_pose *p;
    uint32_t i,j,k;int status;float time;
    if(!v || v->stream!=stream || slot>=SCENE_NANO_VISUAL_CAPACITY)return RF_OK;s=v->slots+slot;
    if(!s->queued || !scene_nano_visual_matches(s,&p) || !scene_nano_visual_enabled(s) ||
       p->playback.generation!=s->generation || (campaign_npc_bodies[s->actor].object_flags&(2u|0x4000u)))return RF_OK;
    for(i=0;i<p->bone_count;i++)if(p->generations[i]!=(uint16_t)s->generation)return RF_OK;
    time=s->seconds*15.f;
    for(i=0;i<2;i++) {
        rf_vfx_mesh *m=v->meshes[i];rf_vfx_instance *instance=s->instances[i];const rf_image *image;
        float opacity;uint32_t rgba;
        status=rf_vfx_instance_update(instance,time);if(status)return status;if(!instance->active)continue;
        status=rf_vfx_material_evaluate(v->materials->data,v->materials->bytes,v->materials->views+i,2,time,&opacity);if(status)return status;
        rgba=0x00ffffffu|((uint32_t)(fmaxf(0,fminf(1,opacity))*255.f+.5f)<<24);if(!(rgba>>24))continue;
        status=rf_vfx_material_texture_sample(&v->textures,v->materials->views+i,i,0,time,0,&image);if(status)return status;
        /* Both MATLs glow1/brightness1 and mesh flag10: unlit additive white.
         * Shared keyed geometry and animated UVs retain authored transforms. */
        for(j=0;j<m->prefix.faces;j++) {
            rf_particle_screen_polygon polygon={0};
            if(m->edges.flags&1) {
                float center[3];if(j)break;
                scene_nano_visual_transform(instance->center,s->position,s->basis,center);
                if(instance->extra[0]<=0 || instance->extra[1]<=0)continue;
                status=scene_cane_flare_project(&stream->particle_camera,center,instance->extra,&polygon);
            }else {
                rf_vfx_face face;rf_vfx_face_output facing;float points[9];rf_particle_billboard_vertex quad[4];
                uint32_t at=m->prefix.face_offset+j*96;
                status=rf_vfx_face_read(m->data+at,m->bytes-at,m->version,&face);if(status)return status;
                if(face.material)return RF_FORMAT;
                for(k=0;k<3;k++)scene_nano_visual_transform(instance->vertices+face.indices[k]*3,s->position,s->basis,points+k*3);
                status=rf_vfx_world_face(&stream->particle_camera,points,(int32_t)i,&facing);
                if(status==RF_RANGE || status==RF_FORMAT)continue;if(status)return status;if(!facing.visible)continue;
                for(k=0;k<3;k++){memcpy(quad[k].position,points+k*3,12);quad[k].uv[0]=instance->uv[j*6+k];quad[k].uv[1]=instance->uv[j*6+3+k];}
                quad[3]=quad[2];status=rf_particle_world_quad(&stream->particle_camera,quad,&polygon);
            }
            if(status)return status;status=scene_nano_visual_submit(stream,&polygon,image,rgba,sink,context);if(status)return status;
        }
    }
    return RF_OK;
}
#endif
