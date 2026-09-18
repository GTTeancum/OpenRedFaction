#ifndef RF_SCENE_APC_SECONDARY_STATE_INC
#define RF_SCENE_APC_SECONDARY_STATE_INC
#include "scene_vehicle_primary.inc"
#define SCENE_APC_SECONDARY_CAPACITY 4u
typedef struct scene_apc_secondary_definition {
    float damage,speed,lifetime,radius,blast,crater,fire_seconds;uint32_t capacity;
} scene_apc_secondary_definition;
typedef struct scene_apc_secondary_round {rf_weapon_flight flight;uint32_t source,driver;float previous[3];} scene_apc_secondary_round;
typedef struct scene_apc_secondary_state {
    scene_apc_secondary_definition definition;float cooldown;int32_t reserve,muzzle;
    scene_apc_secondary_round rounds[SCENE_APC_SECONDARY_CAPACITY];
} scene_apc_secondary_state;
/* Narrow authored APC Rocket admission; generic primary/explosive readers
 * require a Weapon Type clause this installed declaration does not contain.
 * Do not synthesize a magazine or remove its explicit gravity flag. */
static int scene_apc_secondary_read(const void *bytes,uint32_t length,scene_apc_secondary_definition *out)
{
    const unsigned char *text=bytes;scene_apc_secondary_definition v={0};uint32_t at=0,mask=0;int selected=0,found=0;
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
            if(!strcmp(value,"\"APC Rocket\""))selected=found=1;
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
        else if(!strcmp(p,"$Ammo Type")){bit=256;if(strcmp(value,"\"shell\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Flags")){bit=512;if(!strstr(value,"\"gravity\""))return RF_FORMAT;}
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
    if(mask!=2047||v.damage<=0||v.fire_seconds<=0||v.speed<=0||v.lifetime<=0||v.radius<=0||v.blast<=0)return RF_FORMAT;
    *out=v;return RF_OK;
}
static int scene_apc_secondary_open(rf_vpp *tables,const rf_static_model_tags *tags,uint32_t budget,scene_apc_secondary_state *out)
{
    rf_vpp_entry entry;scene_apc_secondary_state value={0};void *text;uint32_t i;int status;
    if(!tables||!tags||!out)return RF_RANGE;
    value.muzzle=-1;
    for(i=0;i<tags->count;i++)if(!strcmp(tags->items[i].name,"secondary_1")){value.muzzle=(int32_t)i;break;}
    if(value.muzzle<0)return RF_NOT_FOUND;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size||entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=scene_apc_secondary_read(text,entry.size,&value.definition);
    free(text);if(status)return status;
    /* Resource owner does NOT fabricate carried rounds: parent admits initial
     * finite inventory (e.g. DEV15) and then assigns reserve explicitly. */
    *out=value;return RF_OK;
}
/* Semi-implicit gravity policy shared with the first-pass grenade integrator,
 * but immediate impact expiration and no bouncing. Errors preserve the round.
 * At60Hz speed/distance match actual authored30m/s and lifetime5s; this is not
 * a claim of exact original rigid-body integration or inherited host speed. */
static int scene_apc_secondary_step(scene_apc_secondary_round *round,float dt,const float gravity[3],
    rf_weapon_flight_sweep sweep,void *context,rf_weapon_flight_event *event)
{
    scene_apc_secondary_round value;uint32_t i;float elapsed;int status;
    if(!round||!gravity||!event||!isfinite(dt)||dt<0||dt>.05f)return RF_RANGE;
    value=*round;elapsed=fminf(dt,(float)value.flight.remaining);
    for(i=0;i<3;i++){
        if(!isfinite(gravity[i]))return RF_RANGE;
        value.previous[i]=value.flight.position[i];
        if(value.flight.active)value.flight.velocity[i]+=gravity[i]*elapsed;
    }
    status=rf_weapon_flight_step(&value.flight,dt,sweep,context,event);if(status)return status;
    *round=value;return RF_OK;
}
#endif
