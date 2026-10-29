#ifndef RF_SCENE_BULLET_TRACERS_INC
#define RF_SCENE_BULLET_TRACERS_INC
/* Bullet tracers for ordinary firearms (player and NPC hitscan shots).
 * Reconstructed from RF.exe:
 *   4c456c..4c45da weapons.tbl "$Tracers:" sets info+264 bit 0x40000000,
 *          "+Tracer Frequency:" info+274, "+Tracer Effect:" info+540 (loaded
 *          into info+548).
 *   4c7871 weapon object creation: with the flag set, the round is a tracer
 *          when the owner's counter (entity+1424, 429f40) is <= 0; a tracer
 *          flies at $Velocity * 0.275 (5a2564) instead of $Velocity.
 *   4c7c46..4c7c5e every created round then runs 429f60 with the weapon's
 *          frequency: decrement the counter and, below zero, reload it.
 *          Frequency 4 gives one tracer in five rounds, 0 every round.
 *   4c754e..4c75ee render: a round flagged as a tracer with a tracer effect
 *          advances that VFX by the frame time, looping (503360), and draws it
 *          at the round's position and orientation (503100).
 *   425dab..425dc7 player rounds start at the view model's Muzzle_1 tag;
 *          NPC rounds at the held weapon's muzzle (41b5a0, rf_scene_npc_muzzle).
 *   Orientation: 4fcea0 basis from the flight direction
 *          (rf_entity_navigation_basis).
 * Tracer01.VFX (meshes.vpp, version 40006) holds two static planes, a
 * Tracer01.tga streak trailing about 5 units behind the round and a
 * LightCorona01.tga glow at it, both additive, visible for effect frames
 * 0..10 (looped). They are drawn by the shared projectile VFX emitter
 * (scene_vfx_emit), the same path as rockets and lasers.
 * Port policy: the port resolves these shots as instant rays, so a tracer is a
 * presentation-only round flying from the muzzle to the ray's end point (first
 * world surface) at the tracer speed. Damage still lands instantly with the
 * ray, not when the tracer arrives; ammunition and RNG are untouched. */
#define SCENE_TRACER_CAPACITY 16u
#define SCENE_TRACER_NPC_COUNTERS 512u
typedef struct scene_tracer {
    float position[3],basis[9],speed,remaining,age;uint32_t active;
} scene_tracer;
static int scene_vfx_emit(scene_stream *s,scene_rocket_visual *v,const float position[3],const float *basis,float time);
/* Tracer01.VFX with its textures merged into the stream materials. */
static scene_rocket_visual *scene_tracer_visual;static float scene_tracer_loop_frames;
static scene_tracer scene_tracers[SCENE_TRACER_CAPACITY];
/* speed: tracer flight (0.275 x $Velocity); velocity/lifetime: the round itself. */
static struct {uint8_t enabled;int32_t frequency;float speed,velocity,lifetime;} scene_tracer_weapons[64];
static int32_t scene_tracer_player_counter,scene_tracer_npc_counter[SCENE_TRACER_NPC_COUNTERS];
/* launched, active, VFX emissions drawn (total), weapons with tracers (+0x10000 = Tracer01.VFX loaded),
 * rounds seen (any weapon), tracers refused (path under .01 or no basis), player rounds seen, last path length *16 */
uint32_t rf_scene_tracers[8];
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
        else if(!strncmp(p,"$Velocity:",10)){scene_tracer_weapons[weapon].velocity=scene_tracer_number(value);scene_tracer_weapons[weapon].speed=scene_tracer_weapons[weapon].velocity*.275f;}
        else if(!strncmp(p,"$Lifetime:",10))scene_tracer_weapons[weapon].lifetime=scene_tracer_number(value);
    }
    free(text);return RF_OK;
}
static void scene_tracer_visual_close(void)
{
    if(!scene_tracer_visual)return;
    rf_vfx_asset_materials_close(&scene_tracer_visual->materials);rf_vfx_geometry_asset_close(&scene_tracer_visual->geometry);
    free(scene_tracer_visual);scene_tracer_visual=NULL;
}
/* Level load, after the weapon table: Tracer01.VFX and its textures, merged
 * into the stream materials as the projectile visuals are. Missing or
 * unsupported data draws no tracer (never fatal). */
static int scene_tracer_visual_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,rf_materials *materials)
{
    scene_rocket_visual *v;uint32_t i,n;rf_material *combined;
    scene_tracer_visual_close();rf_scene_tracers[3]&=0xffffu;scene_tracer_loop_frames=0;
    if(!rf_scene_tracers[3])return RF_OK;
    v=calloc(1,sizeof(*v));if(!v)return RF_IO;scene_tracer_visual=v;
    if(rf_vfx_geometry_asset_open(meshes,"Tracer01.VFX",64*1024,&v->geometry) ||
       rf_vfx_asset_materials_open(v->geometry,maps,map_count,64*1024,&v->materials)){scene_tracer_visual_close();return RF_OK;}
    n=v->materials->textures.texture_count;
    for(i=0;i<v->geometry->count;i++){
        const rf_vfx_mesh_timing *timing=&v->geometry->meshes[i]->prefix.timing;
        if(v->geometry->meshes[i]->prefix.faces>128){scene_tracer_visual_close();return RF_OK;}
        if((timing->end-timing->start)*15>scene_tracer_loop_frames)scene_tracer_loop_frames=(timing->end-timing->start)*15;
    }
    for(i=0;i<n;i++)if(v->materials->textures.textures[i].animation.count!=1){scene_tracer_visual_close();return RF_OK;}
    if(!(scene_tracer_loop_frames>0) || materials->count+n>RF_CAMPAIGN_TEXTURE_SLOTS){scene_tracer_visual_close();return RF_OK;}
    combined=realloc(materials->items,(materials->count+n)*sizeof(*combined));if(!combined)return RF_IO;
    materials->items=combined;v->base=materials->count;
    for(i=0;i<n;i++){
        rf_image *image=v->materials->textures.textures[i].animation.images;
        combined[materials->count]=(rf_material){*image,RF_OK,v->materials->textures.textures[i].animation.archive_index};
        materials->allocated_bytes+=sizeof(*combined)+image->bytes;memset(image,0,sizeof(*image));++materials->count;++materials->loaded;
    }
    rf_scene_tracers[3]|=0x10000u;return RF_OK;
}
/* One created round (4c7871 + 4c7c5e). counter: the owner's tracer counter;
 * start: the muzzle; end: the resolved ray end, clipped here to the first
 * world surface from the muzzle. Non-tracer weapons still run the counter. */
static void scene_tracer_fire(scene_stream *stream,int32_t *counter,int32_t weapon,const float start[3],const float end[3])
{
    float delta[3],length=0,clipped[3],direction[3],matrix[3][3];uint32_t k,slot,best=0;int tracer;
    if(weapon<0 || weapon>=64)return;
    ++rf_scene_tracers[4];if(counter==&scene_tracer_player_counter)++rf_scene_tracers[6];
    tracer=scene_tracer_weapons[weapon].enabled && *counter<=0;
    if(--*counter<0)*counter=scene_tracer_weapons[weapon].frequency;
    if(!tracer || !(scene_tracer_weapons[weapon].speed>0))return;
    memcpy(clipped,end,12);
    if(stream && stream->collision) {
        rf_collision_solid_hit surface;uint32_t matched=0;
        if(!rf_geometry_collision_ray(stream->collision,&campaign_movers,start,end,0x26,&surface,&matched) && matched)
            memcpy(clipped,surface.hit.point,12);
    }
    for(k=0;k<3;k++){delta[k]=clipped[k]-start[k];length+=delta[k]*delta[k];}
    length=sqrtf(length);rf_scene_tracers[7]=isfinite(length)?(uint32_t)(length*16):UINT32_MAX;
    if(!(length>.01f) || !isfinite(length)){++rf_scene_tracers[5];return;}
    for(k=0;k<3;k++)direction[k]=delta[k]/length;
    if(rf_entity_navigation_basis(direction,matrix)){++rf_scene_tracers[5];return;}
    for(slot=0;slot<SCENE_TRACER_CAPACITY;slot++){
        if(!scene_tracers[slot].active){best=slot;break;}
        if(scene_tracers[slot].remaining<scene_tracers[best].remaining)best=slot;
    }
    {scene_tracer *t=scene_tracers+best;memset(t,0,sizeof(*t));
     memcpy(t->position,start,12);memcpy(t->basis,matrix,36);
     t->speed=scene_tracer_weapons[weapon].speed;t->remaining=length;t->active=1;}
    ++rf_scene_tracers[0];
}
/* One game update: fly, and retire on reaching the end point. */
static void scene_tracers_tick(void)
{
    uint32_t i,k,active=0;
    for(i=0;i<SCENE_TRACER_CAPACITY;i++){
        scene_tracer *t=scene_tracers+i;float step;
        if(!t->active)continue;
        step=t->speed*scene_step_seconds;
        if(step>=t->remaining){t->active=0;continue;}
        for(k=0;k<3;k++)t->position[k]+=t->basis[6+k]*step;
        t->remaining-=step;t->age+=scene_step_seconds;++active;
    }
    rf_scene_tracers[1]=active;
}
/* Draw: Tracer01.VFX at each round, effect time looping from creation. */
static int scene_tracers_draw(scene_stream *stream)
{
    uint32_t i;int status;
    if(!scene_tracer_visual)return RF_OK;
    for(i=0;i<SCENE_TRACER_CAPACITY;i++){
        const scene_tracer *t=scene_tracers+i;
        if(!t->active)continue;
        status=scene_vfx_emit(stream,scene_tracer_visual,t->position,t->basis,fmodf(t->age*15,scene_tracer_loop_frames));
        if(status==RF_RANGE)return RF_OK; /* stream full this frame: skip, never fail */
        if(status)return status;
        ++rf_scene_tracers[2];
    }
    return RF_OK;
}
#endif
