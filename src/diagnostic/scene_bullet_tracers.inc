#ifndef RF_SCENE_BULLET_TRACERS_INC
#define RF_SCENE_BULLET_TRACERS_INC
/* Bullet tracers for ordinary firearms (player and NPC hitscan shots).
 * Reconstructed from RF.exe:
 *   4c456c..4c45da weapons.tbl "$Tracers:" sets info+264 bit 0x40000000,
 *          "+Tracer Frequency:" info+274, "+Tracer Effect:" info+540.
 *   4c7871 weapon creation: a round is a tracer when the shooter's counter
 *          (entity+1424, 429f40) is <= 0; its speed is $Velocity * 0.275
 *          (5a2564) instead of $Velocity. 429f60 then decrements the counter
 *          and, below zero, reloads it with the frequency: frequency 4 gives
 *          one tracer in every five rounds, 0 every round.
 *   4c754e draws the round with its tracer effect (Tracer01.VFX).
 * Tracer01.VFX (meshes.vpp): two planes parented to Dummy23. Plane01 is the
 * streak, Tracer01.tga, about 5.2 x .13 units trailing back from the round;
 * Plane02 a glow, LightCorona01.tga, about .9 units across; both additive.
 * Port policy: the port resolves these shots as instant rays, so a tracer is
 * a presentation-only round flying the resolved ray at the tracer speed and
 * stopping at its end point. The two planes are drawn with their original
 * textures and measured sizes as a view-facing streak (never longer than the
 * distance flown) and a view-facing glow, not by evaluating the VFX (its
 * Dummy23 parent transform is not implemented). NPC tracers follow the aim
 * line to the first wall, not the per-shot spread ray. Damage, ammunition
 * and RNG are untouched. */
#define SCENE_TRACER_CAPACITY 16u
#define SCENE_TRACER_NPC_COUNTERS 512u
#define SCENE_TRACER_STREAK_LENGTH 5.2f
#define SCENE_TRACER_STREAK_WIDTH .13f
#define SCENE_TRACER_GLOW_SIZE .9f
typedef struct scene_tracer {
    float position[3],previous[3],direction[3],speed,remaining,flown;uint32_t active,moved;
} scene_tracer;
/* Tracer01.VFX textures merged into the stream materials (stream index + 1; 0 = none). */
static rf_vfx_geometry_asset *scene_tracer_geometry;static rf_vfx_asset_materials *scene_tracer_materials;
static uint32_t scene_tracer_streak_material,scene_tracer_glow_material;
static scene_tracer scene_tracers[SCENE_TRACER_CAPACITY];
static struct {uint8_t enabled;int32_t frequency;float speed;} scene_tracer_weapons[64];
static int32_t scene_tracer_player_counter,scene_tracer_npc_counter[SCENE_TRACER_NPC_COUNTERS];
/* launched, active, streak triangles drawn (total), weapons with tracers (+0x10000 = Tracer01 textures loaded) */
uint32_t rf_scene_tracers[4];
/* Plain decimal ("275.0"); NXDK's strtof/atof are unimplemented asserts. */
static float scene_tracer_number(const char *s)
{
    float value=0,scale=1;int negative=*s=='-';
    if(*s=='-' || *s=='+')++s;
    for(;*s>='0' && *s<='9';++s)value=value*10+(float)(*s-'0');
    if(*s=='.')for(++s;*s>='0' && *s<='9';++s){scale*=.1f;value+=(float)(*s-'0')*scale;}
    return negative?-value:value;
}
/* Level load: weapons.tbl tracer fields for every supplied weapon name. */
static int scene_tracers_load(rf_vpp *tables)
{
    rf_vpp_entry entry;char *text,*line,*next;int32_t weapon=-1;int status;
    memset(scene_tracer_weapons,0,sizeof(scene_tracer_weapons));memset(scene_tracers,0,sizeof(scene_tracers));
    memset(scene_tracer_npc_counter,0,sizeof(scene_tracer_npc_counter));scene_tracer_player_counter=0;rf_scene_tracers[3]=0;
    if(!tables || !campaign_weapon_supply.names.count)return RF_OK;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status==RF_NOT_FOUND?RF_OK:status;
    if(!entry.size || entry.size>256u*1024u)return RF_RANGE;
    text=malloc(entry.size+1u);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status){free(text);return status;}
    text[entry.size]=0;
    for(line=text;line && *line;line=next) {
        char *p=line,*value;
        next=strpbrk(line,"\r\n");if(next){*next++=0;while(*next=='\r' || *next=='\n')++next;}
        while(*p==' ' || *p=='\t')++p;
        if(p[0]=='/' && p[1]=='/')continue;
        value=strchr(p,':');if(!value)continue;++value;while(*value==' ' || *value=='\t')++value;
        if(!strncmp(p,"$Name:",6)) {
            char name[64];size_t n=0;
            if(*value=='"')++value;
            while(value[n] && value[n]!='"' && n+1<sizeof(name)){name[n]=value[n];++n;}
            name[n]=0;weapon=rf_weapon_name_find(&campaign_weapon_supply.names,name);
            if(weapon>=64)weapon=-1;continue;
        }
        if(weapon<0)continue;
        if(!strncmp(p,"$Tracers:",9)){if(!strncmp(value,"true",4)){scene_tracer_weapons[weapon].enabled=1;++rf_scene_tracers[3];}}
        else if(!strncmp(p,"+Tracer Frequency:",18))scene_tracer_weapons[weapon].frequency=(int32_t)scene_tracer_number(value);
        else if(!strncmp(p,"$Velocity:",10))scene_tracer_weapons[weapon].speed=scene_tracer_number(value)*.275f;
    }
    free(text);return RF_OK;
}
/* One fired round. counter: the shooter's tracer counter; start/end: the
 * resolved ray (end clipped to the first world surface here). */
static void scene_tracer_fire(scene_stream *stream,int32_t *counter,int32_t weapon,const float start[3],const float end[3])
{
    float delta[3],length=0,clipped[3];uint32_t k,slot,best=0;int tracer;
    if(weapon<0 || weapon>=64 || !scene_tracer_weapons[weapon].enabled || !(scene_tracer_weapons[weapon].speed>0))return;
    tracer=*counter<=0;
    if(--*counter<0)*counter=scene_tracer_weapons[weapon].frequency;
    if(!tracer)return;
    memcpy(clipped,end,12);
    if(stream && stream->collision) {
        rf_collision_solid_hit surface;uint32_t matched=0;
        if(!rf_geometry_collision_ray(stream->collision,&campaign_movers,start,end,0x26,&surface,&matched) && matched)
            memcpy(clipped,surface.hit.point,12);
    }
    for(k=0;k<3;k++){delta[k]=clipped[k]-start[k];length+=delta[k]*delta[k];}
    length=sqrtf(length);if(!(length>.5f) || !isfinite(length))return;
    for(slot=0;slot<SCENE_TRACER_CAPACITY;slot++){
        if(!scene_tracers[slot].active){best=slot;break;}
        if(scene_tracers[slot].remaining<scene_tracers[best].remaining)best=slot;
    }
    {scene_tracer *t=scene_tracers+best;memset(t,0,sizeof(*t));
     memcpy(t->position,start,12);memcpy(t->previous,start,12);
     for(k=0;k<3;k++)t->direction[k]=delta[k]/length;
     t->speed=scene_tracer_weapons[weapon].speed;t->remaining=length;t->active=1;}
    ++rf_scene_tracers[0];
}
static void scene_tracer_visual_close(void)
{
    rf_vfx_asset_materials_close(&scene_tracer_materials);rf_vfx_geometry_asset_close(&scene_tracer_geometry);
    scene_tracer_streak_material=scene_tracer_glow_material=0;
}
/* Level load, after the weapon table: Tracer01.VFX's two textures join the
 * stream materials. Missing data keeps the untextured streak (never fatal). */
static int scene_tracer_visual_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,rf_materials *materials)
{
    uint32_t m,i,slot[2]={UINT32_MAX,UINT32_MAX},n;rf_material *combined;
    scene_tracer_visual_close();rf_scene_tracers[3]&=0xffffu;
    if(!rf_scene_tracers[3])return RF_OK;
    if(rf_vfx_geometry_asset_open(meshes,"Tracer01.VFX",64*1024,&scene_tracer_geometry))return RF_OK;
    if(rf_vfx_asset_materials_open(scene_tracer_geometry,maps,map_count,64*1024,&scene_tracer_materials)){scene_tracer_visual_close();return RF_OK;}
    for(m=0;m<2 && m<scene_tracer_geometry->count;m++){
        const rf_vfx_mesh *source=scene_tracer_geometry->meshes[m];rf_vfx_face face;uint32_t material;
        if(!source->prefix.faces)continue;
        if(rf_vfx_face_read(source->data+source->prefix.face_offset,source->bytes-source->prefix.face_offset,source->version,&face))continue;
        if(rf_vfx_asset_material_index(scene_tracer_geometry,scene_tracer_materials,m,face.material,&material))continue;
        slot[m]=scene_tracer_materials->textures.bindings[material][0];
    }
    n=scene_tracer_materials->textures.texture_count;
    if(slot[0]>=n || slot[1]>=n){scene_tracer_visual_close();return RF_OK;}
    for(i=0;i<n;i++)if(scene_tracer_materials->textures.textures[i].animation.count!=1){scene_tracer_visual_close();return RF_OK;}
    if(materials->count+n>RF_CAMPAIGN_TEXTURE_SLOTS){scene_tracer_visual_close();return RF_OK;}
    combined=realloc(materials->items,(materials->count+n)*sizeof(*combined));if(!combined)return RF_IO;
    materials->items=combined;
    for(i=0;i<n;i++){
        rf_image *image=scene_tracer_materials->textures.textures[i].animation.images;
        combined[materials->count+i]=(rf_material){*image,RF_OK,scene_tracer_materials->textures.textures[i].animation.archive_index};
        materials->allocated_bytes+=sizeof(*combined)+image->bytes;memset(image,0,sizeof(*image));++materials->loaded;
    }
    scene_tracer_streak_material=materials->count+slot[0]+1;scene_tracer_glow_material=materials->count+slot[1]+1;
    materials->count+=n;rf_scene_tracers[3]|=0x10000u;return RF_OK;
}
/* One game update. */
static void scene_tracers_tick(void)
{
    uint32_t i,k,active=0;
    for(i=0;i<SCENE_TRACER_CAPACITY;i++){
        scene_tracer *t=scene_tracers+i;float step;
        if(!t->active)continue;
        if(t->remaining<=0){t->active=0;continue;} /* reached the end last update; drawn once there */
        step=fminf(t->speed*scene_step_seconds,t->remaining);
        memcpy(t->previous,t->position,12);
        for(k=0;k<3;k++)t->position[k]+=t->direction[k]*step;
        t->remaining-=step;t->flown+=step;t->moved=1;++active;
    }
    rf_scene_tracers[1]=active;
}
/* Draw: the APC tracer streak for each moving round (see scene_apc_primary_draw.inc). */
static int scene_tracers_draw(scene_stream *stream)
{
    uint32_t i,j,k,first,total=0;rf_particle_screen_polygon polygons[SCENE_TRACER_CAPACITY*2];int status;
    uint32_t textured=scene_tracer_streak_material && scene_tracer_glow_material;
    if(!stream||!stream->mesh||!stream->mesh->vertices)return RF_RANGE;
    memset(polygons,0,sizeof(polygons));
    for(i=0;i<SCENE_TRACER_CAPACITY*2;i++){
        const scene_tracer *t=scene_tracers+i/2;float tail[3],a[3],b[3],width;
        if(!t->active || !t->moved)continue;
        if(!textured) {
            if(i&1)continue; /* untextured fallback: the APC streak over this update's travel */
            memcpy(a,t->position,12);memcpy(b,t->previous,12);width=.04f;
        } else if(!(i&1)) {
            float length=fminf(SCENE_TRACER_STREAK_LENGTH,t->flown);
            for(k=0;k<3;k++)tail[k]=t->position[k]-t->direction[k]*length;
            memcpy(a,t->position,12);memcpy(b,tail,12);width=SCENE_TRACER_STREAK_WIDTH;
        } else {
            /* Glow: a square facing the camera, built across the view direction. */
            const float *eye=stream->particle_camera.projection.origin;float view[3],side[3],len=0;
            for(k=0;k<3;k++)view[k]=t->position[k]-eye[k];
            side[0]=view[2];side[1]=0;side[2]=-view[0];
            for(k=0;k<3;k++)len+=side[k]*side[k];len=sqrtf(len);
            if(!(len>1e-4f)){side[0]=1;side[1]=side[2]=0;len=1;}
            for(k=0;k<3;k++){a[k]=t->position[k]+side[k]/len*(SCENE_TRACER_GLOW_SIZE*.5f);b[k]=t->position[k]-side[k]/len*(SCENE_TRACER_GLOW_SIZE*.5f);}
            width=SCENE_TRACER_GLOW_SIZE;
        }
        status=rf_volume_beam_project(&stream->particle_camera,a,b,width,polygons+i);
        if(status){polygons[i].count=0;continue;}
        if(polygons[i].count>12)return RF_RANGE;
        if(polygons[i].count>=3)total+=(polygons[i].count-2)*3;
        for(j=0;j<polygons[i].count;j++){
            const rf_particle_screen_vertex *v=polygons[i].vertices+j;
            if(!isfinite(v->reciprocal_z)||v->reciprocal_z<=0||!isfinite(v->screen[0])||!isfinite(v->screen[1])){polygons[i].count=0;break;}
        }
    }
    if(total>(stream->capacity-stream->mesh->bytes)/sizeof(rf_preview_vertex))return RF_OK; /* no room: skip, never fail */
    first=stream->mesh->count;
    for(i=0;i<SCENE_TRACER_CAPACITY*2;i++)for(j=1;j+1<polygons[i].count;j++){
        uint32_t corners[3]={0,j,j+1};
        for(k=0;k<3;k++){
            const rf_particle_screen_vertex *in=polygons[i].vertices+corners[k];
            rf_preview_vertex *out=stream->mesh->vertices+first++;
            float q=in->reciprocal_z;memset(out,0,sizeof(*out));
            out->position[0]=in->screen[0];out->position[1]=in->screen[1];
            out->position[2]=(1000.f/999.9f)*(1-.1f*q)*16777215.f;
            if(textured){out->color[0]=out->color[1]=out->color[2]=1;}
            else{out->color[0]=1;out->color[1]=.85f;out->color[2]=.45f;}
            out->texture[0]=in->uv[0]*q;out->texture[1]=in->uv[1]*q;out->texture[2]=q;
            out->material=textured?((i&1)?scene_tracer_glow_material:scene_tracer_streak_material)-1:UINT32_MAX;
            out->lightmap=RF_PREVIEW_ADDITIVE_FADE_TAG|(textured?255u:220u);
            out->lightmap_texture[2]=q;
        }
        ++rf_scene_tracers[2];
    }
    stream->mesh->count=first;stream->mesh->bytes=first*sizeof(rf_preview_vertex);return RF_OK;
}
#endif
