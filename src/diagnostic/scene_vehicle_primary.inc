#ifndef RF_SCENE_VEHICLE_PRIMARY_INC
#define RF_SCENE_VEHICLE_PRIMARY_INC
#include "rf/entity_assets.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Bounded first-pass APC/Jeep clipless primary scheduler. Installed table
 * values, not invented magazines/infinite ammo. Caller owns finite reserve,
 * source/occupant admission, aimed direction and real covered damage/flight.
 * APC uses actual muzzle_1. Jeep needs its attached gun muzzle owner, not the
 * chassis jeep_gun attachment. No tank/fighter/secondary or turret animation.
 * Generic primary parser currently rejects these nonexplosive clipless guns,
 * hence this narrow reader. NXDK-safe decimal conversion avoids strtod.
 */
typedef struct scene_vehicle_primary_definition {
    float damage,fire_seconds,start_seconds,speed,lifetime,radius,spread;
    uint32_t capacity;int32_t damage_kind;
} scene_vehicle_primary_definition;
typedef struct scene_vehicle_primary_state {float cooldown,warmup;uint32_t held,shots;} scene_vehicle_primary_state;
typedef int (*scene_vehicle_primary_dispatch)(void *,uint32_t,
    const scene_vehicle_primary_definition *,const float muzzle[12],const float direction[3],uint32_t *accepted);
static int scene_vehicle_primary_number(const char *p,float *out)
{
    double value=0,scale=.1;uint32_t digits=0;
    while(*p==' '||*p=='\t')++p;
    while(*p>='0'&&*p<='9'){value=value*10+(*p++-'0');++digits;if(value>1000000)return RF_RANGE;}
    if(*p=='.'){++p;while(*p>='0'&&*p<='9'){value+=(*p++-'0')*scale;scale*=.1;++digits;}}
    while(*p==' '||*p=='\t')++p;
    if(!digits||*p||value>1000000)return RF_FORMAT;
    *out=(float)value;return RF_OK;
}
static int scene_vehicle_primary_read(const void *bytes,uint32_t length,const char *name,scene_vehicle_primary_definition *out)
{
    const unsigned char *text=bytes;scene_vehicle_primary_definition v={0};uint32_t at=0,mask=0;int selected=0,found=0;
    if(!bytes||!out||!name)return RF_RANGE;
    if(strcmp(name,"APC Minigun")&&strcmp(name,"Jeep Gun"))return RF_NOT_FOUND;
    while(at<length){char line[512],*p,*end,*value;uint32_t n=0,bit=0;float *number=NULL;int quoted=0,status;
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
            if(selected)break;
            value=p+6;while(*value==' '||*value=='\t')++value;
            if(*value=='"'&&strlen(value)==strlen(name)+2&&!strncmp(value+1,name,strlen(name))){selected=found=1;}
            continue;
        }
        if(!selected)continue;
        if(!strcmp(p,"#End"))break;
        value=strchr(p,':');if(!value)continue;*value++=0;
        while(*value==' '||*value=='\t')++value;
        if(!strcmp(p,"$Damage")){bit=1;number=&v.damage;}
        else if(!strcmp(p,"$Fire Wait")){bit=2;number=&v.fire_seconds;}
        else if(!strcmp(p,"$Velocity")){bit=4;number=&v.speed;}
        else if(!strcmp(p,"$Lifetime")){bit=8;number=&v.lifetime;}
        else if(!strcmp(p,"$Collision Radius")){bit=16;number=&v.radius;}
        else if(!strcmp(p,"$Spread Degrees")){bit=32;number=&v.spread;}
        else if(!strcmp(p,"$Start Delay")){bit=256;number=&v.start_seconds;}
        else if(!strcmp(p,"$Damage Type")){bit=64;if(strcmp(value,"\"armor piercing bullet\""))return RF_FORMAT;v.damage_kind=2;}
        else if(!strcmp(p,"$Ammo Type")){bit=128;if(strcmp(value,"\"turret_ammo\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Max Ammo")){
            float capacity,mp;char *split=value;bit=512;
            while(*split&&*split!=' '&&*split!='\t')++split;
            if(!*split)return RF_FORMAT;
            *split++=0;
            if(scene_vehicle_primary_number(value,&capacity)||scene_vehicle_primary_number(split,&mp)||capacity<1||capacity>65535||floorf(capacity)!=capacity)return RF_FORMAT;
            v.capacity=(uint32_t)capacity;
        }else if(!strcmp(p,"$Clip Size")||!strcmp(p,"$Clip Reload Time"))return RF_FORMAT;
        if(mask&bit)return RF_FORMAT;
        if(number){status=scene_vehicle_primary_number(value,number);if(status)return status;}
        mask|=bit;
    }
    if(!found)return RF_NOT_FOUND;
    if((mask&767)!=767||v.damage<=0||v.fire_seconds<.05f||v.fire_seconds>60||v.speed<=0||v.lifetime<=0||v.spread>90||v.start_seconds>60)return RF_FORMAT;
    *out=v;return RF_OK;
}
static int scene_vehicle_primary_load(rf_vpp *tables,const char *name,uint32_t budget,scene_vehicle_primary_definition *out)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables||!out)return RF_RANGE;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size||entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=scene_vehicle_primary_read(text,entry.size,name,out);
    free(text);return status;
}
/* Resource-phase APC admission; no round creation. Parent initializes its
 * finite reserve from the admitted entity inventory/capacity policy. */
static int scene_vehicle_primary_apc_open(rf_vpp *tables,const rf_static_model_tags *tags,uint32_t budget,
    scene_vehicle_primary_definition *definition,scene_vehicle_primary_state *state,int32_t *muzzle)
{
    scene_vehicle_primary_definition v;uint32_t i;int32_t index=-1;int status;
    if(!tags||!definition||!state||!muzzle)return RF_RANGE;
    for(i=0;i<tags->count;i++)if(!strcmp(tags->items[i].name,"muzzle_1")){index=(int32_t)i;break;}
    if(index<0)return RF_NOT_FOUND;
    status=scene_vehicle_primary_load(tables,"APC Minigun",budget,&v);if(status)return status;
    *definition=v;memset(state,0,sizeof(*state));*muzzle=index;return RF_OK;
}
/* At most one shot per <=50ms step; residual cooldown preserves60Hz cadence.
 * Callback accepted0 (e.g. full pool) spends nothing. On callback error state
 * and reserve are unchanged; dispatch must not publish then return failure.
 * Both table guns are from_eye: direction is caller's admitted aim, muzzle
 * transform is actual tag pose for shot origin/effects. NULL aim uses authored
 * tag forward for fixed-gun first pass; explicit aim supports from_eye later.
 */
static int scene_vehicle_primary_tick(scene_vehicle_primary_state *state,const scene_vehicle_primary_definition *definition,
    const rf_static_model_tags *tags,int32_t muzzle_tag,const float position[3],const float basis[9],const float aim[3],
    uint32_t source,uint32_t held,uint32_t allowed,float dt,int32_t *reserve,scene_vehicle_primary_dispatch dispatch,void *context,uint32_t *fired)
{
    scene_vehicle_primary_state next;float pose[12],direction[3],length=0;uint32_t i,accepted=0;int status;
    if(!state||!definition||!reserve||!fired||!isfinite(dt)||dt<0||dt>.05f||
       !isfinite(state->cooldown)||!isfinite(state->warmup)||state->warmup<0||
       !isfinite(definition->fire_seconds)||definition->fire_seconds<.05f||
       !isfinite(definition->start_seconds)||definition->start_seconds<0||
       *reserve<0||(uint32_t)*reserve>definition->capacity)return RF_RANGE;
    next=*state;next.cooldown=fmaxf(-dt,next.cooldown-dt);
    if(!held||!allowed){next.held=0;next.warmup=0;*state=next;*fired=0;return RF_OK;}
    if(!next.held){next.held=1;next.warmup=0;}
    next.warmup=fminf(definition->start_seconds,next.warmup+dt);
    if(next.warmup+1.e-6f<definition->start_seconds||next.cooldown>1.e-6f||!*reserve){*state=next;*fired=0;return RF_OK;}
    if(!tags||!position||!basis||!dispatch||source==UINT32_MAX)return RF_RANGE;
    status=rf_static_model_tag_place(tags,muzzle_tag,basis,position,pose);if(status)return status;
    if(!aim)aim=pose+6;
    for(i=0;i<3;i++){if(!isfinite(aim[i]))return RF_RANGE;length+=aim[i]*aim[i];}
    if(!isfinite(length)||length<1.e-12f)return RF_RANGE;
    length=sqrtf(length);for(i=0;i<3;i++)direction[i]=aim[i]/length;
    status=dispatch(context,source,definition,pose,direction,&accepted);if(status)return status;
    if(accepted>1)return RF_FORMAT;
    if(accepted){--*reserve;++next.shots;next.cooldown=fmaxf(0,next.cooldown+definition->fire_seconds);}
    *state=next;*fired=accepted;return RF_OK;
}
#endif
