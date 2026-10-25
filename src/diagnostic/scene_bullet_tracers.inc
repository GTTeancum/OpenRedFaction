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
 * Port policy (first pass): the port resolves these shots as instant rays, so
 * a tracer is a presentation-only round flying the resolved ray at the tracer
 * speed and stopping at its end point. It is drawn as the APC tracer streak
 * (previous to current position, .04 m, amber-white additive), not a decoded
 * Tracer01.VFX mesh. NPC tracers follow the aim line to the first wall, not
 * the per-shot spread ray. Damage, ammunition and RNG are untouched. */
#define SCENE_TRACER_CAPACITY 16u
#define SCENE_TRACER_NPC_COUNTERS 512u
typedef struct scene_tracer {
    float position[3],previous[3],direction[3],speed,remaining;uint32_t active,moved;
} scene_tracer;
static scene_tracer scene_tracers[SCENE_TRACER_CAPACITY];
static struct {uint8_t enabled;int32_t frequency;float speed;} scene_tracer_weapons[64];
static int32_t scene_tracer_player_counter,scene_tracer_npc_counter[SCENE_TRACER_NPC_COUNTERS];
/* launched, active, streak triangles drawn (total), weapons with tracers */
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
        t->remaining-=step;t->moved=1;++active;
    }
    rf_scene_tracers[1]=active;
}
/* Draw: the APC tracer streak for each moving round (see scene_apc_primary_draw.inc). */
static int scene_tracers_draw(scene_stream *stream)
{
    uint32_t i,j,k,first,total=0;rf_particle_screen_polygon polygons[SCENE_TRACER_CAPACITY];int status;
    if(!stream||!stream->mesh||!stream->mesh->vertices)return RF_RANGE;
    memset(polygons,0,sizeof(polygons));
    for(i=0;i<SCENE_TRACER_CAPACITY;i++){
        const scene_tracer *t=scene_tracers+i;
        if(!t->active || !t->moved)continue;
        status=rf_volume_beam_project(&stream->particle_camera,t->position,t->previous,.04f,polygons+i);
        if(status)return status;
        if(polygons[i].count>12)return RF_RANGE;
        if(polygons[i].count>=3)total+=(polygons[i].count-2)*3;
        for(j=0;j<polygons[i].count;j++){
            const rf_particle_screen_vertex *v=polygons[i].vertices+j;
            if(!isfinite(v->reciprocal_z)||v->reciprocal_z<=0||!isfinite(v->screen[0])||!isfinite(v->screen[1])){polygons[i].count=0;break;}
        }
    }
    if(total>(stream->capacity-stream->mesh->bytes)/sizeof(rf_preview_vertex))return RF_OK; /* no room: skip, never fail */
    first=stream->mesh->count;
    for(i=0;i<SCENE_TRACER_CAPACITY;i++)for(j=1;j+1<polygons[i].count;j++){
        uint32_t corners[3]={0,j,j+1};
        for(k=0;k<3;k++){
            const rf_particle_screen_vertex *in=polygons[i].vertices+corners[k];
            rf_preview_vertex *out=stream->mesh->vertices+first++;
            float q=in->reciprocal_z;memset(out,0,sizeof(*out));
            out->position[0]=in->screen[0];out->position[1]=in->screen[1];
            out->position[2]=(1000.f/999.9f)*(1-.1f*q)*16777215.f;
            out->color[0]=1;out->color[1]=.85f;out->color[2]=.45f;
            out->texture[0]=in->uv[0]*q;out->texture[1]=in->uv[1]*q;out->texture[2]=q;
            out->material=UINT32_MAX;out->lightmap=RF_PREVIEW_ADDITIVE_FADE_TAG|220u;
            out->lightmap_texture[2]=q;
        }
        ++rf_scene_tracers[2];
    }
    stream->mesh->count=first;stream->mesh->bytes=first*sizeof(rf_preview_vertex);return RF_OK;
}
#endif
