#ifndef RF_SCENE_SKYBOX_INCLUDED
#define RF_SCENE_SKYBOX_INCLUDED
/* Original4d4b90 selects the first primary sky room (file+28 -> room+1).
 * Borrow immutable authored geometry and the current scene material owner.
 * No decoded pixels, GPU vertices, heap blocks or archive handles are owned. */
#define SCENE_SKYBOX_FACES 1024u
#define SCENE_SKYBOX_SCRATCH 8192u
static struct {
    scene_stream *stream;const rf_geometry *geometry;
    const rf_scene_world_geometry *world;
    rf_scene_skybox_state state;
    float center[3];uint32_t room,count,ready;int32_t last_ms;
    struct {uint32_t face,material;} faces[SCENE_SKYBOX_FACES];
} scene_skybox;
/* status, charged bytes, faces, triangles, borrowed image bytes, free pages,
 * ONs, refused ONs, ticks, draws, submitted triangles, submitted vertices,
 * captures, restores, clock rebases, resident-resource refusals. */
uint32_t rf_scene_skybox[16];
#define SCENE_SKYBOX_CHARGE ((((sizeof(scene_skybox)+sizeof(rf_scene_skybox)+\
    sizeof(((rf_runtime_triggers *)0)->skybox_state)+sizeof(((rf_runtime_triggers *)0)->skybox_context)+4095u)/4096u)*4096u)+SCENE_SKYBOX_SCRATCH)
_Static_assert(SCENE_SKYBOX_CHARGE<=64u*1024u,"skybox metadata/scratch cap");
_Static_assert(sizeof(rf_scene_skybox_state)==32,"skybox portable state values");
#ifdef RF_IMAGE_XBOX_NATIVE
extern uint32_t rf_xbox_available_memory_pages(void);
#endif
static uint32_t scene_sky_hash(uint32_t hash,const void *data,uint32_t bytes)
{const unsigned char *p=data;while(bytes--)hash=(hash^*p++)*16777619u;return hash;}
static uint32_t scene_sky_word(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void scene_skybox_close(void)
{memset(&scene_skybox,0,sizeof(scene_skybox));}
static int scene_skybox_phase(int32_t now,float *out)
{
    int32_t elapsed;double phase;
    if(!out||now<0||now>RF_TIMER_PERIOD||scene_skybox.last_ms<0||scene_skybox.last_ms>RF_TIMER_PERIOD)return RF_RANGE;
    elapsed=now-scene_skybox.last_ms;if(elapsed<0)elapsed+=RF_TIMER_PERIOD;
    /* Half-period rejects a backwards/replaced clock, rather than interpreting
     * it as days of forward motion. Repeated/paused ticks do not advance. */
    if(elapsed>RF_TIMER_PERIOD/2)return RF_RANGE;
    phase=scene_skybox.state.phase;
    if(scene_skybox.state.rate>0)phase+=scene_skybox.state.rate*((double)elapsed*.001);
    phase=fmod(phase,360.0);if(!isfinite(phase)||phase<0)return RF_RANGE;
    *out=(float)phase;if(*out>=360)*out=0;return RF_OK;
}
static void scene_skybox_tick(int32_t now)
{
    float phase;int status;if(!scene_skybox.stream||now==scene_skybox.last_ms)return;
    status=scene_skybox_phase(now,&phase);
    if(!status)scene_skybox.state.phase=phase;
    else {rf_scene_skybox[0]=(uint32_t)status;++rf_scene_skybox[14];}
    if(now>=0&&now<=RF_TIMER_PERIOD)scene_skybox.last_ms=now;++rf_scene_skybox[8];
}
static int scene_skybox_command(void *context,const rf_level_event *event,int32_t now)
{
    unsigned axis;if(!event||context!=scene_skybox.stream){++rf_scene_skybox[7];return RF_OK;}
    axis=(unsigned char)event->texts[0][0];if(axis>='A'&&axis<='Z')axis+=32;
    if(!scene_skybox.state.present||event->texts[0][1]||axis<'x'||axis>'z'||
       !isfinite(event->values[0])||fabsf(event->values[0])>3600||now<0||now>RF_TIMER_PERIOD){
        ++rf_scene_skybox[7];return RF_OK;
    }
    scene_skybox_tick(now);scene_skybox.state.axis=axis;scene_skybox.state.rate=event->values[0];
    ++rf_scene_skybox[6];return RF_OK;
}
int rf_scene_skybox_validate(const rf_scene_skybox_state *s)
{
    const rf_scene_skybox_state *live=&scene_skybox.state;
    if(!s||!scene_skybox.stream)return RF_NOT_FOUND;
    if(s->version!=1||s->present>1||s->level_identity!=live->level_identity||s->present!=live->present||
       s->room_uid!=live->room_uid||s->source_identity!=live->source_identity)return RF_FORMAT;
    if(s->axis<'x'||s->axis>'z'||!isfinite(s->rate)||fabsf(s->rate)>3600||
       !isfinite(s->phase)||s->phase<0||s->phase>=360)return RF_FORMAT;
    if(!s->present&&(s->axis!='x'||s->rate!=0||s->phase!=0))return RF_FORMAT;
    return RF_OK;
}
int rf_scene_skybox_capture(int32_t now,rf_scene_skybox_state *out)
{
    rf_scene_skybox_state next;int status;
    if(!out||!scene_skybox.stream)return RF_NOT_FOUND;next=scene_skybox.state;
    status=scene_skybox_phase(now,&next.phase);if(status)return status;
    status=rf_scene_skybox_validate(&next);if(status)return status;
    *out=next;++rf_scene_skybox[12];return RF_OK;
}
void rf_scene_skybox_restore(const rf_scene_skybox_state *state,int32_t now)
{
    /* Caller validated this candidate/clock against the same target before
     * successful storage close. Final publication cannot do fallible work. */
    scene_skybox.state=*state;scene_skybox.last_ms=now;++rf_scene_skybox[13];
}
static int scene_skybox_open(scene_stream *stream,const rf_level *level,const rf_scene_world_geometry *world)
{
    const rf_geometry *g;uint32_t room,i,j,hash;int status;
    if(!stream||!level||scene_skybox.stream)return RF_RANGE;
    memset(rf_scene_skybox,0,sizeof(rf_scene_skybox));rf_scene_skybox[1]=SCENE_SKYBOX_CHARGE;
    scene_skybox.stream=stream;scene_skybox.last_ms=0;scene_skybox.state.version=1;
    scene_skybox.state.axis='x';scene_skybox.state.room_uid=UINT32_MAX;
    hash=scene_sky_hash(2166136261u,level->entry.name,(uint32_t)strlen(level->entry.name));
    scene_skybox.state.level_identity=scene_sky_hash(hash,&level->timestamp,4);
    if(!world||!(g=world->world)||!g->data||!world->offsets||!world->slots||!stream->materials)return RF_NOT_FOUND;
    for(room=0;room<g->rooms;room++)if(g->data[g->room_offsets[room]+28]&&!g->data[g->room_offsets[room]+34])break;
    if(room==g->rooms)return RF_OK;
    if(world->geometry_count<1||world->offsets[1]<world->offsets[0]||world->offsets[1]-world->offsets[0]!=g->textures)return RF_FORMAT;
    hash=scene_sky_hash(2166136261u,g->data+g->room_offsets[room],40);
    for(i=0;i<3;i++){
        float lo,hi;memcpy(&lo,g->data+g->room_offsets[room]+4+i*4,4);memcpy(&hi,g->data+g->room_offsets[room]+16+i*4,4);
        if(!isfinite(lo)||!isfinite(hi)||lo>=hi)return RF_FORMAT;scene_skybox.center[i]=lo*.5f+hi*.5f;
    }
    for(i=0;i<g->faces;i++){
        rf_geometry_face face;uint32_t slot;char name[256];const rf_material *m;
        status=rf_geometry_get_face(g,i,&face);if(status)return status;
        if(face.room!=room||face.portal||(face.flags&1)||face.texture==UINT32_MAX)continue;
        if(scene_skybox.count==SCENE_SKYBOX_FACES||face.texture>=g->textures||face.corners<3||face.corners>128)return RF_RANGE;
        slot=world->slots[world->offsets[0]+face.texture];
        if(slot>=world->material_count||slot>=stream->materials->count||!stream->materials->items)return RF_FORMAT;
        m=stream->materials->items+slot;
        if(m->status||!m->image.rgba||!m->image.width||!m->image.height||
           m->image.width>4096||m->image.height>4096||
           (m->image.width&(m->image.width-1))||(m->image.height&(m->image.height-1))||
           (uint64_t)m->image.width*m->image.height*(rf_image_is_packed_1555(&m->image)?2u:4u)!=m->image.bytes)return RF_NOT_FOUND;
        /* No fallback texture, hidden decode or image-copy bank. This owner
         * resolves the live material descriptor again at each submission. */
        for(j=0;j<scene_skybox.count;j++)if(scene_skybox.faces[j].material==slot)break;
        if(j==scene_skybox.count)rf_scene_skybox[4]+=m->image.bytes;
        scene_skybox.faces[scene_skybox.count].face=i;scene_skybox.faces[scene_skybox.count++].material=slot;
        rf_scene_skybox[3]+=face.corners-2;
        hash=scene_sky_hash(hash,g->data+g->face_offsets[i],56);
        status=rf_geometry_texture_name(g,face.texture,name,sizeof(name));if(status)return status;
        hash=scene_sky_hash(hash,name,(uint32_t)strlen(name));
        for(j=0;j<face.corners;j++){
            rf_geometry_corner corner;float point[3];
            status=rf_geometry_get_corner(g,i,j,&corner);if(status)return status;
            status=rf_geometry_vertex(g,corner.vertex,point);if(status)return status;
            hash=scene_sky_hash(hash,point,12);hash=scene_sky_hash(hash,corner.uv,8);
        }
    }
    if(!scene_skybox.count)return RF_NOT_FOUND;
    scene_skybox.geometry=g;scene_skybox.world=world;scene_skybox.room=room;
    scene_skybox.state.present=1;scene_skybox.state.source_identity=hash;
    scene_skybox.state.room_uid=scene_sky_word(g->data+g->room_offsets[room]);
    scene_skybox.ready=1;rf_scene_skybox[2]=scene_skybox.count;return RF_OK;
}
static void scene_skybox_open_optional(scene_stream *stream,const rf_level *level)
{
    int status=scene_skybox_open(stream,level,actor_follow_world);rf_scene_skybox[0]=(uint32_t)status;
    if(status){scene_skybox.ready=0;++rf_scene_skybox[15];}
}
uint32_t rf_scene_skybox_ready(void)
{
    if(!scene_skybox.ready||!scene_skybox.stream||scene_skybox.stream!=particle_draw_stream)return 0;
#ifdef RF_IMAGE_XBOX_NATIVE
    /* Recheck after renderer/world allocations, at actual draw admission.
     * This path itself allocates zero bytes and cannot spend the reserve. */
    rf_scene_skybox[5]=rf_xbox_available_memory_pages();if(rf_scene_skybox[5]<4096)return 0;
#endif
    return 1;
}
int rf_scene_draw_skybox(rf_scene_particle_sink sink,void *context)
{
    const rf_geometry *g=scene_skybox.geometry;scene_stream *stream=scene_skybox.stream;
    rf_visibility_camera camera={0};rf_visibility_camera_parameters parameters={{640,480,0,0,1,1,1000,1},{0},{0},.1f,1,1,1,0};
    float rotation[9]={1,0,0,0,1,0,0,0,1},sine,cosine;uint32_t i,j,k;int status;
    if(!sink||!rf_scene_skybox_ready())return RF_OK;
    if(!g||scene_skybox.world!=actor_follow_world||actor_follow_world->world!=g)return RF_FORMAT;
    memcpy(parameters.origin,scene_skybox.center,12);memcpy(parameters.basis,stream->particle_camera.view.basis,36);
    status=rf_visibility_camera_setup(&parameters,&camera);if(status)return status;
    /*45e870 builds the axis matrix;4d39b0 installs its inverse for the sky
     * room's row-vector transform. Geometry is translated around its center;
     * gameplay-camera translation never moves this backdrop. */
    if(scene_skybox.state.rate>0){
        float angle=scene_skybox.state.phase*0.01745329238474369f;sine=sinf(angle);cosine=cosf(angle);
        if(scene_skybox.state.axis=='x'){rotation[4]=rotation[8]=cosine;rotation[5]=-sine;rotation[7]=sine;}
        else if(scene_skybox.state.axis=='y'){rotation[0]=rotation[8]=cosine;rotation[2]=sine;rotation[6]=-sine;}
        else {rotation[0]=rotation[4]=cosine;rotation[1]=-sine;rotation[3]=sine;}
    }
    ++rf_scene_skybox[9];
    for(i=0;i<scene_skybox.count;i++){
        rf_geometry_face face;uint32_t slot=scene_skybox.faces[i].material;const rf_image *image;
        status=rf_geometry_get_face(g,scene_skybox.faces[i].face,&face);if(status)return status;
        if(!rf_preview_plane_visible(face.plane,scene_skybox.center))continue;
        if(slot>=stream->materials->count||!stream->materials->items||stream->materials->items[slot].status)return RF_FORMAT;
        image=&stream->materials->items[slot].image;if(!image->rgba)return RF_NOT_FOUND;
        for(j=1;j+1<face.corners;j++){
            uint32_t ids[3]={0,j,j+1};rf_particle_billboard_vertex corners[4];rf_particle_screen_polygon polygon={0};
            rf_particle_draw_vertex vertices[12];rf_particle_vertex_environment env={0};
            for(k=0;k<3;k++){
                rf_geometry_corner c;float point[3],delta[3];uint32_t axis;
                status=rf_geometry_get_corner(g,scene_skybox.faces[i].face,ids[k],&c);if(status)return status;
                status=rf_geometry_vertex(g,c.vertex,point);if(status)return status;
                for(axis=0;axis<3;axis++)delta[axis]=point[axis]-scene_skybox.center[axis];
                for(axis=0;axis<3;axis++)corners[k].position[axis]=scene_skybox.center[axis]+
                    rotation[axis*3]*delta[0]+rotation[axis*3+1]*delta[1]+rotation[axis*3+2]*delta[2];
                memcpy(corners[k].uv,c.uv,8);
            }
            corners[3]=corners[2];status=rf_particle_world_quad(&camera,corners,&polygon);if(status)return status;
            if(!polygon.count)continue;if(polygon.count>12)return RF_RANGE;
            env.rgba=UINT32_MAX;env.vertex_color=env.vertex_alpha=1;
            env.depth_scale=env.reciprocal_scale=camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
            for(k=0;k<polygon.count;k++){status=rf_particle_vertex_encode(&env,polygon.vertices+k,vertices+k);if(status)return status;}
            status=sink(context,vertices,polygon.count,image,RF_SKYBOX_MODE);if(status)return status;
            ++rf_scene_skybox[10];rf_scene_skybox[11]+=polygon.count;
        }
    }
    return RF_OK;
}
#endif
