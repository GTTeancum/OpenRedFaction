#ifndef SCENE_FIGHTER_WEAPON_INC
#define SCENE_FIGHTER_WEAPON_INC
#include "scene_submarine_homing.inc"
/* Fighter uses the shared clipless scheduler and straight-flight core.
 * Actual rocket model is DrillMissile01.VFX, not a static substitute.
 * Minigun undeviating flag: caller must not add random spread despite table
 * Spread Degrees1.5. Homing reuses explicitly provisional turn/cone policy. */
static int scene_fighter_rocket_read(const void *bytes,uint32_t length,scene_submarine_weapon_definition *out)
{
    const unsigned char *text=bytes;scene_submarine_weapon_definition v={0};uint32_t at=0,mask=0;int selected=0,found=0;
    if(!bytes||!out)return RF_RANGE;
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
            if(!strcmp(value,"\"Fighter Rocket\""))selected=found=1;
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
        else if(!strcmp(p,"$Damage Radius")){bit=32;number=&v.blast;}
        else if(!strcmp(p,"+Crater Radius")){bit=64;number=&v.crater;}
        else if(!strcmp(p,"$Damage Type")){bit=128;if(strcmp(value,"\"explosive\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Ammo Type")){bit=256;if(strcmp(value,"\"15cm rocket\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Flags")){bit=512;if(strstr(value,"\"gravity\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Homing")){bit=2048;if(strcmp(value,"true"))return RF_FORMAT;}
        else if(!strcmp(p,"+Turn Time")){bit=4096;number=&v.turn_seconds;}
        else if(!strcmp(p,"+View Cone")){bit=8192;number=&v.view_degrees;}
        else if(!strcmp(p,"+Scanning Range")){bit=16384;number=&v.scan_range;}
        else if(!strcmp(p,"+Wakeup Time")){bit=32768;number=&v.wakeup;}
        else if(!strcmp(p,"$V3D Filename")){bit=65536;if(strcmp(value,"\"DrillMissile01.VFX\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Max Ammo")){
            float capacity,mp;char *split=value;bit=1024;
            while(*split&&*split!=' '&&*split!='\t')++split;
            if(!*split)return RF_FORMAT;
            *split++=0;
            if(scene_vehicle_primary_number(value,&capacity)||scene_vehicle_primary_number(split,&mp)||capacity<1||capacity>65535||floorf(capacity)!=capacity)return RF_FORMAT;
            v.capacity=(uint32_t)capacity;
        }
        if(mask&bit)return RF_FORMAT;
        if(number){status=scene_vehicle_primary_number(value,number);if(status)return status;}
        mask|=bit;
    }
    if(!found)return RF_NOT_FOUND;
    if(mask!=131071||v.damage<=0||v.fire_seconds<=0||v.speed<=0||v.lifetime<=0||v.radius<=0||v.blast<=0||
       v.turn_seconds<=0||v.view_degrees<=0||v.view_degrees>360||v.scan_range<=0||v.wakeup<0)return RF_FORMAT;
    *out=v;return RF_OK;
}

typedef struct scene_fighter_weapon_state {
    scene_vehicle_primary_definition primary;
    scene_vehicle_primary_state primary_fire,rocket_fire;
    scene_submarine_weapon_definition rocket;
    int32_t primary_reserve,rocket_reserve,primary_muzzle,rocket_muzzle;
    scene_submarine_weapon_round rockets[4];uint32_t driver;
} scene_fighter_weapon_state;
static int scene_fighter_weapon_open(rf_vpp *tables,const rf_static_model_tags *tags,
    const char *primary_tag,const char *rocket_tag,uint32_t budget,scene_fighter_weapon_state *out)
{
    scene_fighter_weapon_state value={0};rf_vpp_entry entry;void *text;uint32_t i;int status;
    if(!tables||!tags||!primary_tag||!rocket_tag||!out)return RF_RANGE;
    value.primary_muzzle=value.rocket_muzzle=-1;
    for(i=0;i<tags->count;i++){
        if(!strcmp(tags->items[i].name,primary_tag))value.primary_muzzle=(int32_t)i;
        if(!strcmp(tags->items[i].name,rocket_tag))value.rocket_muzzle=(int32_t)i;
    }
    if(value.primary_muzzle<0||value.rocket_muzzle<0)return RF_NOT_FOUND;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size||entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=scene_vehicle_primary_read(text,entry.size,"Fighter Minigun",&value.primary);
    if(!status)status=scene_fighter_rocket_read(text,entry.size,&value.rocket);
    free(text);if(status)return status;
    *out=value;return RF_OK; /* Caller assigns admitted finite inventory. */
}
static int scene_fighter_rocket_dispatch(void *context,uint32_t source,const scene_vehicle_primary_definition *definition,
    const float muzzle[12],const float direction[3],uint32_t *accepted)
{
    scene_fighter_weapon_state *state=context;scene_submarine_weapon_round round={0};uint32_t i;int status;
    for(i=0;i<4&&state->rockets[i].flight.active;i++){}
    if(i==4){*accepted=0;return RF_OK;}
    status=rf_weapon_flight_launch(&round.flight,muzzle+9,direction,definition->speed,definition->lifetime,definition->radius);
    if(status)return status;
    round.source=source;round.driver=state->driver;round.liquid.query_flags=0x1004;memcpy(round.previous,muzzle+9,12);
    state->rockets[i]=round;*accepted=1;return RF_OK;
}
static int scene_fighter_weapon_fire(scene_fighter_weapon_state *state,const rf_static_model_tags *tags,
    const float position[3],const float basis[9],const float *aim,uint32_t source,uint32_t driver,
    uint32_t secondary,uint32_t held,uint32_t allowed,float dt,
    scene_vehicle_primary_dispatch primary_dispatch,void *context,uint32_t *fired)
{
    scene_vehicle_primary_definition rocket={0};int status;uint32_t old_driver;
    if(!state||secondary>1)return RF_RANGE;
    if(!secondary)return scene_vehicle_primary_tick(&state->primary_fire,&state->primary,tags,state->primary_muzzle,
        position,basis,aim,source,held,allowed,dt,&state->primary_reserve,primary_dispatch,context,fired);
    rocket.damage=state->rocket.damage;rocket.damage_kind=3;rocket.capacity=state->rocket.capacity;
    rocket.fire_seconds=state->rocket.fire_seconds;rocket.speed=state->rocket.speed;
    rocket.lifetime=state->rocket.lifetime;rocket.radius=state->rocket.radius;
    old_driver=state->driver;state->driver=driver;
    status=scene_vehicle_primary_tick(&state->rocket_fire,&rocket,tags,state->rocket_muzzle,
        position,basis,aim,source,held,allowed,dt,&state->rocket_reserve,scene_fighter_rocket_dispatch,state,fired);
    if(status)state->driver=old_driver;
    return status;
}
/* Fighter targets need not be underwater. This bridge admits the candidate's
 * existing alive/hostile fields while retaining the common guidance math. */
typedef struct scene_fighter_homing_reader {scene_submarine_homing_reader reader;void *context;} scene_fighter_homing_reader;
static int scene_fighter_homing_read(void *context,uint32_t index,scene_submarine_homing_candidate *out)
{
    scene_fighter_homing_reader *r=context;int status=r->reader(r->context,index,out);
    if(!status)out->submerged=1;
    return status;
}
static int scene_fighter_rocket_step(scene_submarine_weapon_round *round,const scene_submarine_weapon_definition *definition,
    float dt,scene_submarine_homing_reader reader,void *reader_context,uint32_t count,
    scene_submarine_homing_los los,void *los_context,rf_weapon_flight_liquid_sweep sweep,void *sweep_context,
    rf_weapon_flight_liquid_event *out,uint32_t *target)
{
    scene_submarine_weapon_round value;scene_fighter_homing_reader bridge={reader,reader_context};
    rf_weapon_flight_liquid_policy policy={0,0,-1,-1};rf_weapon_flight_liquid_event event;uint32_t selected;int status;
    if(!round||!out||!target||(count&&!reader))return RF_RANGE;
    value=*round;
    status=scene_submarine_homing_scan(&value,definition,dt,scene_fighter_homing_read,&bridge,count,los,los_context,&selected);if(status)return status;
    memcpy(value.previous,value.flight.position,12);
    /* Ordinary rocket liquid policy, never the torpedo-only stop flag. */
    status=rf_weapon_flight_step_liquid(&value.flight,dt,&value.liquid,&policy,sweep,sweep_context,&event);if(status)return status;
    *round=value;*out=event;*target=selected;return RF_OK;
}
#endif
