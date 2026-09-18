#ifndef RF_SCENE_DRILLER_WEAPON_INC
#define RF_SCENE_DRILLER_WEAPON_INC
#include "rf/entity_assets.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Installed Drill is continuous bash, with NO ammunition. 421310 supplies
 * range3.5, contact warmup.75, accepted-cut cap25, centerY+.78, radius10/20
 * and single/double templates. The ordinary primary reader requires a clip,
 * so this bounded reader handles only this authored clipless declaration.
 * First-pass policy: damage30 per .5 seconds after startdelay1; two bits
 * striking the same object deal one dose. Damage/cut providers retain scene
 * ownership, cover, material admission and actual template publication. */
typedef struct scene_driller_weapon {
    float damage,fire_seconds,start_seconds,radius;
    float held_seconds,damage_wait,contact_seconds,spin,phase;
    int32_t tags[2];uint32_t held,contact_active,accepted_cuts,contacts,damage_calls;
} scene_driller_weapon;
typedef struct scene_driller_weapon_backend {
    void *context;
    int (*trace)(void *,uint32_t,const float *,const float *,float,rf_weapon_flight_contact *,uint32_t *);
    int (*damage)(void *,const rf_weapon_flight_contact *,float,uint32_t);
    /* center3, full host basis9, template, requested radius, flags, source,
     * accepted. 467020 flag8 forces geometric scale1; radius is NOT scale. */
    int (*cut)(void *,const float *,const float *,const char *,float,uint32_t,uint32_t,uint32_t *);
} scene_driller_weapon_backend;
/* Positive decimal subset used by these four fields; no libc strtod on NXDK. */
static int scene_driller_weapon_decimal(const char *p,float *out)
{
    double value=0,scale=.1;uint32_t digits=0;
    while(*p==' '||*p=='\t')++p;
    while(*p>='0'&&*p<='9'){value=value*10+(*p++-'0');++digits;if(value>1000000)return RF_RANGE;}
    if(*p=='.'){++p;while(*p>='0'&&*p<='9'){value+=(*p++-'0')*scale;scale*=.1;++digits;}}
    while(*p==' '||*p=='\t')++p;
    if(!digits||*p||value<=0||value>1000000)return RF_FORMAT;
    *out=(float)value;return RF_OK;
}
static int scene_driller_weapon_read(const void *bytes,uint32_t length,scene_driller_weapon *out)
{
    const unsigned char *text=bytes;scene_driller_weapon v={0};uint32_t at=0,mask=0;int selected=0,found=0;
    if(!bytes||!out)return RF_RANGE;
    while(at<length){char line[512],*p,*end,*value;uint32_t n=0;int quoted=0,status;
        while(at<length&&text[at]!='\r'&&text[at]!='\n'){
            if(n+1>=sizeof(line))return RF_RANGE;
            line[n++]=(char)text[at++];
        }
        while(at<length&&(text[at]=='\r'||text[at]=='\n'))++at;
        line[n]=0;
        for(p=line;*p;p++){if(*p=='"')quoted=!quoted;if(!quoted&&p[0]=='/'&&p[1]=='/'){*p=0;break;}}
        p=line;while(*p==' '||*p=='\t')++p;
        end=p+strlen(p);while(end>p&&(end[-1]==' '||end[-1]=='\t'))*--end=0;
        if(!strncmp(p,"$Name:",6)){
            value=p+6;while(*value==' '||*value=='\t')++value;
            if(selected)break;
            if(!strcmp(value,"\"Drill\"")){selected=1;found=1;}
            continue;
        }
        if(!selected)continue;
        if(!strcmp(p,"#End"))break;
        value=strchr(p,':');if(!value)continue;*value++=0;
        while(*value==' '||*value=='\t')++value;
        {uint32_t bit=0;float *number=NULL;
         if(!strcmp(p,"$Damage")){bit=1;number=&v.damage;}
         else if(!strcmp(p,"$Fire Wait")){bit=2;number=&v.fire_seconds;}
         else if(!strcmp(p,"$Start Delay")){bit=4;number=&v.start_seconds;}
         else if(!strcmp(p,"$Collision Radius")){bit=8;number=&v.radius;}
         else if(!strcmp(p,"$Ammo Type")){bit=16;if(strcmp(value,"\"\""))return RF_FORMAT;}
         else if(!strcmp(p,"$Damage Type")){bit=32;if(strcmp(value,"\"bash\""))return RF_FORMAT;}
         if(bit&&mask&bit)return RF_FORMAT;
         if(number){status=scene_driller_weapon_decimal(value,number);if(status)return status;}
         mask|=bit;}
    }
    if(!found)return RF_NOT_FOUND;
    if(mask!=63||v.fire_seconds>60||v.start_seconds>60||v.radius>10)return RF_FORMAT;
    v.tags[0]=v.tags[1]=-1;*out=v;return RF_OK;
}
static int scene_driller_weapon_open(rf_vpp *tables,const rf_static_model_tags *tags,
    uint32_t budget,scene_driller_weapon *out)
{
    rf_vpp_entry entry;scene_driller_weapon v;void *text;uint32_t i;int status;
    if(!tables||!tags||!out)return RF_RANGE;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size||entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=scene_driller_weapon_read(text,entry.size,&v);
    free(text);if(status)return status;
    for(i=0;i<tags->count;i++){
        if(!strcmp(tags->items[i].name,"primary_weapon_1"))v.tags[0]=(int32_t)i;
        if(!strcmp(tags->items[i].name,"primary_weapon_2"))v.tags[1]=(int32_t)i;
    }
    if(v.tags[0]<0||v.tags[1]<0)return RF_NOT_FOUND;
    *out=v;return RF_OK;
}
static int scene_driller_weapon_tick(scene_driller_weapon *state,const rf_static_model_tags *tags,
    const float position[3],const float basis[9],uint32_t source,uint32_t held,float dt,
    const scene_driller_weapon_backend *backend)
{
    rf_weapon_flight_contact hits[2];uint32_t count=0,i,k,terrain_count=0;
    float center[3]={0},forward[3],speed_limit;uint32_t limit_bits=0x412fede0u;int status;
    if(!state||!tags||!position||!basis||!backend||!backend->trace||!backend->damage||!backend->cut||
       !isfinite(dt)||dt<0||dt>.1f)return RF_RANGE;
    memcpy(&speed_limit,&limit_bits,4);
    state->spin=fminf(speed_limit,fmaxf(0,state->spin+dt*speed_limit*(held?1.f:-.5f)));
    state->phase=fmodf(state->phase+dt*state->spin,6.283185307f);
    if(!held){state->held=0;state->held_seconds=state->damage_wait=state->contact_seconds=0;state->contact_active=0;return RF_OK;}
    state->held=1;state->held_seconds+=dt;
    if(state->held_seconds<state->start_seconds)return RF_OK;
    state->damage_wait=fmaxf(0,state->damage_wait-dt);
    for(k=0;k<3;k++)forward[k]=basis[6+k];
    for(i=0;i<2;i++){
        float pose[12],end[3];uint32_t hit=0;
        status=rf_static_model_tag_place(tags,state->tags[i],basis,position,pose);if(status)return status;
        for(k=0;k<3;k++)end[k]=pose[9+k]+3.5f*forward[k];
        status=backend->trace(backend->context,source,pose+9,end,state->radius,hits+count,&hit);if(status)return status;
        if(hit){++count;++state->contacts;}
    }
    if(!count){state->contact_active=0;state->contact_seconds=0;return RF_OK;}
    if(state->damage_wait<=0){
        for(i=0;i<count;i++)if(hits[i].object!=UINT32_MAX&&(!i||hits[i].object!=hits[0].object)){
            status=backend->damage(backend->context,hits+i,state->damage,source);if(status)return status;
            ++state->damage_calls;
        }
        state->damage_wait=state->fire_seconds;
    }
    for(i=0;i<count;i++)if(hits[i].object==UINT32_MAX){
        ++terrain_count;for(k=0;k<3;k++)center[k]+=hits[i].hit.point[k];
    }
    if(!terrain_count){state->contact_active=0;state->contact_seconds=0;return RF_OK;}
    if(!state->contact_active){state->contact_active=1;state->contact_seconds=0;return RF_OK;}
    state->contact_seconds+=dt;
    if(state->contact_seconds>=.75f&&state->accepted_cuts<25){uint32_t accepted=0;
        for(k=0;k<3;k++)center[k]/=(float)terrain_count;
        center[1]+=.78f;
        status=backend->cut(backend->context,center,basis,terrain_count==2?"bit_driller_double.v3d":"bit_driller_single.v3d",
            (float)terrain_count*10,0x3e,source,&accepted);if(status)return status;
        if(accepted)++state->accepted_cuts;
        state->contact_active=0;state->contact_seconds=0;
    }
    return RF_OK;
}
#endif
