#ifndef RF_SCENE_PLAYER_FOOTSTEPS_INCLUDED
#define RF_SCENE_PLAYER_FOOTSTEPS_INCLUDED

/* First-person branch of42f940 ->42fb20. This is a distance accumulator over
 * the actual support-relative body velocity, not a model marker or timer.
 * game.tbl433eb4..433efc owns pan and both stride distances; entity.tbl
 *41c781..41c8a3 owns material, climb-kind and crawl groups. No additional PCM
 * bank or device voice owner is introduced. Single-player only. The original
 *408280 AI hearing stimulus and492330 pooled-surface overlay remain separate;
 * this pass implements the ordinary local-player audio branch, not complete
 * original frame/RNG scheduling or save persistence of these global counters.
 */
typedef struct scene_player_footstep_state {
    int32_t ground[10],climb[3],crawl;
    float distance,climb_distance,pan,travel,climb_travel;
    uint32_t ready,reports;
} scene_player_footstep_state;
static scene_player_footstep_state scene_player_footstep;
uint32_t rf_scene_player_footsteps[16]; /* ticks,eligible,crossings,requests,starts,failures,material,group,sample,voice,status,mode,side,RNG,skipped,ready */
float rf_scene_player_footstep_values[8]; /* ground stride,climb stride,next pan,ground travel,climb travel,speed,dt,last requested pan */
uint32_t rf_scene_player_footstep_events[16][12]; /* frame,mode,material,group,sample,voice,status,side,travel bits,speed bits,pan bits,RNG */
char rf_scene_player_footstep_sample[64];

static void scene_player_footsteps_reset(void)
{
    memset(&scene_player_footstep,0,sizeof(scene_player_footstep));
    memset(scene_player_footstep.ground,0xff,sizeof(scene_player_footstep.ground));
    memset(scene_player_footstep.climb,0xff,sizeof(scene_player_footstep.climb));
    scene_player_footstep.crawl=-1;
    memset(rf_scene_player_footsteps,0,sizeof(rf_scene_player_footsteps));
    memset(rf_scene_player_footstep_values,0,sizeof(rf_scene_player_footstep_values));
    memset(rf_scene_player_footstep_events,0,sizeof(rf_scene_player_footstep_events));
    memset(rf_scene_player_footstep_sample,0,sizeof(rf_scene_player_footstep_sample));
    rf_scene_player_footsteps[6]=rf_scene_player_footsteps[7]=
        rf_scene_player_footsteps[8]=rf_scene_player_footsteps[9]=UINT32_MAX;
}

static int scene_player_footstep_same(const char *a,const char *b)
{
    while(*a && *b) {
        unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+='a'-'A';
        if(y>='A' && y<='Z')y+='a'-'A';
        if(x!=y)return 0;
    }
    return *a==*b;
}

/* Bounded installed-table adapter. Comments are ignored only outside quoted
 * names; the input need not be NUL terminated. No input pointer is retained.
 * Entity lines are at most363 bytes in the original table.
 */
static int scene_player_footstep_line(const unsigned char *text,uint32_t bytes,
    uint32_t *at,char line[512],char **key,char **value)
{
    uint32_t n=0;char *p,*end;int quoted=0;
    if(*at>=bytes)return RF_NOT_FOUND;
    while(*at<bytes && text[*at]!='\r' && text[*at]!='\n') {
        if(n+1>=512)return RF_RANGE;
        line[n++]=(char)text[(*at)++];
    }
    while(*at<bytes && (text[*at]=='\r' || text[*at]=='\n'))++*at;
    line[n]=0;
    for(p=line;*p;p++) {
        if(*p=='"')quoted=!quoted;
        if(!quoted && p[0]=='/' && p[1]=='/'){*p=0;break;}
    }
    p=line;while(*p==' ' || *p=='\t')++p;
    end=p+strlen(p);while(end>p && (end[-1]==' ' || end[-1]=='\t'))*--end=0;
    *key=p;*value=strchr(p,':');
    if(*value) {
        *(*value)++=0;
        while(**value==' ' || **value=='\t')++*value;
    }
    return RF_OK;
}

static int scene_player_footstep_name(char *value,char **name)
{
    size_t n;if(!value || (n=strlen(value))<2 || value[0]!='"' || value[n-1]!='"')return RF_FORMAT;
    value[n-1]=0;*name=value+1;return RF_OK;
}

static int scene_player_footsteps_load(rf_vpp *tables,const void *entity_text,
    uint32_t entity_bytes,const char *class_name)
{
    scene_player_footstep_state next={0};rf_vpp_entry entry;void *text=NULL;
    uint32_t at=0,mask=0,i;int selected=0,found=0,status;
    char line[512],*key,*value;
    memset(next.climb,0xff,sizeof(next.climb));next.crawl=-1;
    status=rf_entity_footstep_groups_read(entity_text,entity_bytes,class_name,&campaign_foley,next.ground);
    if(status)return status;
    while((status=scene_player_footstep_line(entity_text,entity_bytes,&at,line,&key,&value))==RF_OK) {
        char *name;int32_t group;
        if(scene_player_footstep_same(key,"$Name")) {
            if(selected)break;
            status=scene_player_footstep_name(value,&name);if(status)return status;
            selected=scene_player_footstep_same(name,class_name);found|=selected;
            continue;
        }
        if(!selected || !value)continue;
        if(!scene_player_footstep_same(key,"$Climb Footstep Sounds") &&
           !scene_player_footstep_same(key,"$Crawl Footstep Sound"))continue;
        status=scene_player_footstep_name(value,&name);if(status)return status;
        status=rf_foley_find(&campaign_foley,name,&group);if(status)return status;
        if(scene_player_footstep_same(key,"$Crawl Footstep Sound"))next.crawl=group;
        else if(group>=0) {
            if((uint32_t)group>=campaign_foley.group_count || campaign_foley.groups[group].material>=3)return RF_FORMAT;
            next.climb[campaign_foley.groups[group].material]=group;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;
    status=rf_vpp_find(tables,"game.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>65536)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
    at=0;
    while((status=scene_player_footstep_line(text,entry.size,&at,line,&key,&value))==RF_OK) {
        uint32_t bit=0;float *number=NULL;
        if(scene_player_footstep_same(key,"$Player Footstep Pan")){bit=1;number=&next.pan;}
        else if(scene_player_footstep_same(key,"$Player Step Distance")){bit=2;number=&next.distance;}
        else if(scene_player_footstep_same(key,"$Player Step Distance Climb")){bit=4;number=&next.climb_distance;}
        if(!bit)continue;
        if(!value || (mask&bit)){status=RF_FORMAT;goto done;}
        /* Existing bounded NXDK decimal adapter, no libc strtod dependency. */
        status=scene_vehicle_primary_number(value,number);if(status)goto done;
        mask|=bit;
    }
    if(status!=RF_NOT_FOUND)goto done;
    if(mask!=7 || next.pan>1 || next.distance<=0 || next.climb_distance<=0){status=RF_FORMAT;goto done;}
    /* The table reader already bound each group; validate compact views now,
     * rather than doing any archive/table work in the movement loop. */
    for(i=0;i<14;i++) {
        int32_t group=i<10?next.ground[i]:i<13?next.climb[i-10]:next.crawl;
        if(group>=0) {
            const rf_foley_group *g;
            if((uint32_t)group>=campaign_foley.group_count){status=RF_RANGE;goto done;}
            g=campaign_foley.groups+group;
            if(g->count>INT32_MAX || g->first>campaign_foley.sample_count ||
               g->count>campaign_foley.sample_count-g->first){status=RF_FORMAT;goto done;}
        }
    }
    next.ready=1;scene_player_footstep=next;rf_scene_player_footsteps[15]=1;
    rf_scene_player_footstep_values[0]=next.distance;
    rf_scene_player_footstep_values[1]=next.climb_distance;
    rf_scene_player_footstep_values[2]=next.pan;
    printf("PLAYER_FOOTSTEP_READY class%s distance%.9g climb%.9g pan%.9g\n",
        class_name,next.distance,next.climb_distance,next.pan);
    status=RF_OK;
done:
    free(text);return status;
}

static void scene_player_footstep_play(uint32_t frame,uint32_t mode,int32_t material,
    int32_t group,float travel,float speed)
{
    const rf_foley_group *g;rf_audio_group_input input={0};rf_audio_group_request request;
    uint32_t first,half,available,*event,side=scene_player_footstep.pan>=0;int status=RF_OK;int32_t voice=-1;
    ++rf_scene_player_footsteps[3];
    rf_scene_player_footsteps[6]=(uint32_t)material;rf_scene_player_footsteps[7]=(uint32_t)group;
    rf_scene_player_footsteps[8]=rf_scene_player_footsteps[9]=UINT32_MAX;
    rf_scene_player_footsteps[12]=side;rf_scene_player_footstep_sample[0]=0;
    rf_scene_player_footstep_values[7]=scene_player_footstep.pan;
    if(group<0 || (uint32_t)group>=campaign_foley.group_count){status=RF_NOT_FOUND;goto done;}
    g=campaign_foley.groups+group;half=g->count/2;first=g->first+(side?half:0);
    available=g->count-(side?half:0);
    input.object_kind=0;input.player_present=1;input.view_mode=0;
    memcpy(input.position,scene_actor_body.state.position,12);
    input.parameters[0]=1;input.parameters[1]=scene_player_footstep.pan;
    /*48a930 keeps a count<=1 half at index0, including an odd singleton.
     * Flat request scalars are pan then volume; spatial requests differ. */
    if(!campaign_foley.samples || !available)status=RF_NOT_FOUND;
    else status=rf_audio_group_choose(&input,campaign_foley.samples+first,available,
        (int32_t)half,&combat_sound_random,&request);
    if(!status) {
        rf_scene_player_footsteps[8]=(uint32_t)request.sample;
        if(request.sample<0 || (uint32_t)request.sample>=campaign_audio_bank.count)status=RF_NOT_FOUND;
        else {
            const char *name=campaign_audio_bank.samples[request.sample].name;
            memcpy(rf_scene_player_footstep_sample,name,strlen(name)+1);
            voice=campaign_sound_start(request.sample,input.position,request.parameters[1],
                request.group,request.spatial,request.parameters[0]);
            if(voice<0)status=RF_IO;
        }
    }
done:
    if(status)++rf_scene_player_footsteps[5];else ++rf_scene_player_footsteps[4];
    rf_scene_player_footsteps[9]=(uint32_t)voice;rf_scene_player_footsteps[10]=(uint32_t)status;
    rf_scene_player_footsteps[13]=combat_sound_random.value;
    event=rf_scene_player_footstep_events[(rf_scene_player_footsteps[3]-1)%16];
    event[0]=frame;event[1]=mode;event[2]=(uint32_t)material;event[3]=(uint32_t)group;
    event[4]=rf_scene_player_footsteps[8];event[5]=(uint32_t)voice;event[6]=(uint32_t)status;event[7]=side;
    memcpy(event+8,&travel,4);memcpy(event+9,&speed,4);memcpy(event+10,&scene_player_footstep.pan,4);
    event[11]=combat_sound_random.value;
    if(scene_player_footstep.reports<16 || (status && rf_scene_player_footsteps[5]<=4)) {
        ++scene_player_footstep.reports;
        printf("PLAYER_FOOTSTEP frame%u mode%u material%d group%d file%s travel%.9g speed%.9g pan%.9g starts%u status%d\n",
            frame,mode,material,group,rf_scene_player_footstep_sample[0]?rf_scene_player_footstep_sample:"<none>",
            travel,speed,scene_player_footstep.pan,rf_scene_player_footsteps[4],status);
    }
}

static void scene_player_footsteps_tick(scene_stream *stream,uint32_t frame,
    uint32_t walkable,int32_t material)
{
    scene_player_footstep_state *s=&scene_player_footstep;
    uint32_t mode=rf_scene_actor_landing[1];float speed;double length;int32_t group;
    ++rf_scene_player_footsteps[0];rf_scene_player_footsteps[11]=mode;
    /*42f940 parent gate,42fb20 dead gate, and actual first-person ownership.
     * Flight/swim, unsupported ground, vehicles and cinematic views do not
     * manufacture travel or consume deferred footsteps. */
    if(!s->ready || !campaign_spawn || !scene_actor_body.allocated_bytes ||
       campaign_player_object.view!=&campaign_player_view || campaign_player_view.linked_handle!=-1 ||
       (campaign_player_view.flags_810&1u) || scene_driller_active(stream) || campaign_cutscene_runtime.active ||
       (mode!=1 && mode!=2) || (mode==1 && !walkable))goto skipped;
    if(mode==1 && rf_scene_actor_movement_settings.mode!=0 && rf_scene_actor_movement_settings.mode!=1 &&
       !(rf_scene_actor_stance_flags&0x400u))goto skipped;
    if(mode==2 && (!campaign_climb.region || campaign_climb.region->kind<0 || campaign_climb.region->kind>=3))goto skipped;
    length=((double)scene_actor_body.state.velocity[0]*scene_actor_body.state.velocity[0]+
        (double)scene_actor_body.state.velocity[1]*scene_actor_body.state.velocity[1])+
        (double)scene_actor_body.state.velocity[2]*scene_actor_body.state.velocity[2];
    speed=(float)sqrt(length);
    if(!isfinite(speed) || !isfinite(scene_step_seconds) || scene_step_seconds<0)goto skipped;
    ++rf_scene_player_footsteps[1];
    rf_scene_player_footstep_values[5]=speed;rf_scene_player_footstep_values[6]=scene_step_seconds;
    if(mode==2) {
        if(s->climb_travel>s->climb_distance) {
            ++rf_scene_player_footsteps[2];
            group=s->climb[campaign_climb.region->kind];
            if(group>=0)scene_player_footstep_play(frame,mode,campaign_climb.region->kind,group,s->climb_travel,speed);
            s->pan=-s->pan;s->climb_travel=0; /*42fc52..42fc67*/
        }
        s->travel=0;s->climb_travel=(float)((double)s->climb_travel+(double)speed*scene_step_seconds);
    } else {
        if(s->travel>s->distance) {
            ++rf_scene_player_footsteps[2];
            if(material<0)material=0;
            if(material>=10)goto skipped;
            if(rf_scene_actor_stance_flags&0x1000u) {
                group=s->ground[4]; /*42fd13..42fd44: shallow-water override. */
                if(group<0)goto skipped;
            } else {
                group=s->ground[material];if(group<0)group=s->ground[0];
                if(group<0)goto skipped;
            }
            if(rf_scene_actor_stance_flags&0x400u)group=s->crawl; /*40a130/42fd81*/
            scene_player_footstep_play(frame,mode,material,group,s->travel,speed);
            s->pan=-s->pan;
            /* Original42fea7 is threshold minus old travel, including sign;
             * do not replace it with a conventional positive remainder. */
            s->travel=s->distance-s->travel;
        }
        s->climb_travel=0;s->travel=(float)((double)s->travel+(double)speed*scene_step_seconds);
    }
    rf_scene_player_footstep_values[2]=s->pan;rf_scene_player_footstep_values[3]=s->travel;
    rf_scene_player_footstep_values[4]=s->climb_travel;return;
skipped:
    ++rf_scene_player_footsteps[14];
}
#endif
