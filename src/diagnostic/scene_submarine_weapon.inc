#ifndef RF_SCENE_SUBMARINE_WEAPON_INC
#define RF_SCENE_SUBMARINE_WEAPON_INC
#include "scene_vehicle_primary.inc"
#include "scene_driller_resources.inc"
/* Static torpedo01 owner reuses established material merge/world draw/close
 * helpers. No fabricated corpse/vehicle metadata or attachment required. */
static int scene_submarine_weapon_model_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,scene_driller_resources **out)
{
    scene_driller_resources *o=NULL;rf_model_file *file=NULL;uint32_t used;int status;
    if(!meshes||!maps||!map_count||!out||*out||budget<sizeof(*o)+sizeof(*file))return RF_RANGE;
    o=calloc(1,sizeof(*o));file=malloc(sizeof(*file));if(!o||!file){status=RF_IO;goto fail;}
    strcpy(o->model,"torpedo01.v3m");o->seat=-1;used=sizeof(*o)+sizeof(*file);
    status=rf_model_file_open(file,meshes,o->model);if(status)goto fail;
    status=rf_static_render_resource_open(file,budget-used,&o->render);if(status)goto fail;used+=o->render.allocated_bytes;
    status=rf_model_materials_open(&o->materials,file,maps,map_count,budget-used);if(status)goto fail;
    o->peak_bytes=used+o->materials.peak_bytes;o->resident_bytes=used-sizeof(*file)+o->materials.resident_bytes;
    free(file);*out=o;return RF_OK;
fail:free(file);scene_driller_resources_close(&o);return status;
}
#define SCENE_SUBMARINE_WEAPON_CAPACITY 4u
typedef struct scene_submarine_weapon_definition {
    float damage,speed,lifetime,radius,blast,crater,fire_seconds;float turn_seconds,view_degrees,scan_range,wakeup;uint32_t capacity;
} scene_submarine_weapon_definition;
typedef struct scene_submarine_weapon_round {rf_weapon_flight flight;uint32_t source,driver;float previous[3];rf_weapon_flight_liquid_state liquid;} scene_submarine_weapon_round;
typedef struct scene_submarine_weapon_state {
    scene_submarine_weapon_definition definition;float cooldown;int32_t reserve,muzzle;
    scene_submarine_weapon_round rounds[SCENE_SUBMARINE_WEAPON_CAPACITY];
} scene_submarine_weapon_state;
/* Narrow authored Torpedo admission; generic primary/explosive readers
 * require a Weapon Type clause this installed declaration does not contain.
 * No fabricated magazine; retain all authored homing constants. */
static int scene_submarine_weapon_read(const void *bytes,uint32_t length,scene_submarine_weapon_definition *out)
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
            if(!strcmp(value,"\"Torpedo\""))selected=found=1;
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
        else if(!strcmp(p,"$Ammo Type")){bit=256;if(strcmp(value,"\"torpedo\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Flags")){bit=512;if(!strstr(value,"\"underwater\"") || !strstr(value,"\"torpedo\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Homing")){bit=2048;if(strcmp(value,"true"))return RF_FORMAT;}
        else if(!strcmp(p,"+Turn Time")){bit=4096;number=&v.turn_seconds;}
        else if(!strcmp(p,"+View Cone")){bit=8192;number=&v.view_degrees;}
        else if(!strcmp(p,"+Scanning Range")){bit=16384;number=&v.scan_range;}
        else if(!strcmp(p,"+Wakeup Time")){bit=32768;number=&v.wakeup;}
        else if(!strcmp(p,"$V3D Filename")){bit=65536;if(strcmp(value,"\"torpedo01.v3d\""))return RF_FORMAT;}
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
/* Caller selects an actual named muzzle from the retained Sub model tags. */
static int scene_submarine_weapon_open(rf_vpp *tables,const rf_static_model_tags *tags,const char *muzzle_name,
    uint32_t budget,scene_submarine_weapon_state *out)
{
    scene_submarine_weapon_state value={0};rf_vpp_entry entry;void *text;uint32_t i;int status;
    if(!tables||!tags||!muzzle_name||!out)return RF_RANGE;
    value.muzzle=-1;for(i=0;i<tags->count;i++)if(!strcmp(tags->items[i].name,muzzle_name)){value.muzzle=(int32_t)i;break;}
    if(value.muzzle<0)return RF_NOT_FOUND;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size||entry.size>budget)return RF_RANGE;text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(!status)status=scene_submarine_weapon_read(text,entry.size,&value.definition);
    free(text);if(status)return status;*out=value;return RF_OK; /* Parent supplies finite reserve. */
}
typedef int (*scene_submarine_weapon_water)(void *,const float position[3],uint32_t *underwater);
/* Optional recovered homing backend: supplies normalized-or-normalizable next
 * direction, preserving speed. NULL explicitly means straight first-pass flight,
 * NOT implemented target acquisition/homing. Metadata is retained for that work. */
typedef int (*scene_submarine_weapon_steer)(void *,const scene_submarine_weapon_definition *,
    const rf_weapon_flight *,float dt,float direction[3],uint32_t *changed);
static int scene_submarine_weapon_fire(scene_submarine_weapon_state *s,const rf_static_model_tags *tags,
    const float position[3],const float basis[9],const float *aim,uint32_t source,uint32_t driver,
    uint32_t held,uint32_t allowed,float dt,scene_submarine_weapon_water water,void *context,uint32_t *fired)
{
    scene_submarine_weapon_round round={0};float pose[12],cooldown;uint32_t i,wet;int status;
    if(!s||!fired||!isfinite(dt)||dt<0||dt>.05f||!isfinite(s->cooldown)||s->reserve<0||
       (uint32_t)s->reserve>s->definition.capacity)return RF_RANGE;
    cooldown=fmaxf(0,s->cooldown-dt);
    if(!held||!allowed||cooldown>1e-6f||!s->reserve){s->cooldown=cooldown;*fired=0;return RF_OK;}
    if(!water||!tags||!position||!basis||source==UINT32_MAX)return RF_RANGE;
    for(i=0;i<SCENE_SUBMARINE_WEAPON_CAPACITY;i++)if(!s->rounds[i].flight.active)break;
    if(i==SCENE_SUBMARINE_WEAPON_CAPACITY){s->cooldown=cooldown;*fired=0;return RF_OK;}
    status=rf_static_model_tag_place(tags,s->muzzle,basis,position,pose);if(status)return status;
    status=water(context,pose+9,&wet);if(status)return status;if(wet>1)return RF_FORMAT;
    if(!wet){s->cooldown=cooldown;*fired=0;return RF_OK;}
    status=rf_weapon_flight_launch(&round.flight,pose+9,aim?aim:pose+6,s->definition.speed,s->definition.lifetime,s->definition.radius);if(status)return status;
    round.source=source;round.driver=driver;round.liquid.query_flags=0x1004;memcpy(round.previous,pose+9,12);
    s->rounds[i]=round;--s->reserve;s->cooldown=s->definition.fire_seconds;*fired=1;return RF_OK;
}
/* Returns ordinary flight event once. Impact caller dispatches direct explosive
 * damage200 plus blast/crater5 from definition, effect charge_explode radius2,
 * retaining source/driver exclusions. Caller must include liquid boundary in
 * sweep and use rf_weapon_liquid_contact(flag0x10000) for torpedo expiration;
 * liquid expiration is not a solid-impact blast. No scene/world side effects. */
static inline int scene_submarine_weapon_step(scene_submarine_weapon_round *round,const scene_submarine_weapon_definition *definition,
    float dt,rf_weapon_flight_sweep sweep,void *context,scene_submarine_weapon_steer steer,rf_weapon_flight_event *event)
{
    scene_submarine_weapon_round value;rf_weapon_flight_event result;float direction[3],norm=0;uint32_t i,changed=0;int status;
    if(!round||!definition||!event||!isfinite(dt)||dt<0||dt>.05f)return RF_RANGE;value=*round;
    memcpy(value.previous,value.flight.position,12);
    if(value.flight.active&&steer&&definition->lifetime-(float)value.flight.remaining>=definition->wakeup){
        status=steer(context,definition,&value.flight,dt,direction,&changed);if(status)return status;if(changed>1)return RF_FORMAT;
        if(changed){for(i=0;i<3;i++){if(!isfinite(direction[i]))return RF_FORMAT;norm+=direction[i]*direction[i];}
            if(!isfinite(norm)||norm<1e-12f)return RF_FORMAT;
            for(i=0;i<3;i++)value.flight.velocity[i]=direction[i]*definition->speed/sqrtf(norm);}
    }
    status=rf_weapon_flight_step(&value.flight,dt,sweep,context,&result);if(status)return status;
    *round=value;*event=result;return RF_OK;
}
#endif
