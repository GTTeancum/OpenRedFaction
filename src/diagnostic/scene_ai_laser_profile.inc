#ifndef RF_SCENE_AI_LASER_PROFILE_INCLUDED
#define RF_SCENE_AI_LASER_PROFILE_INCLUDED
/* Exact installed nonexplosive Laser profile. The generic primary reader
 * deliberately rejects this clipless/noncontinuous family; do not broaden
 * its magazine contract or feed Laser through the explosive reader. */
typedef struct scene_ai_laser_profile {
    rf_weapon_primary_definition primary;
    float speed,lifetime,radius;int32_t weapon;uint32_t ready,liquid_weapon_flags;
} scene_ai_laser_profile;
/* Direct-flight payload shared by exact Laser, delayed TriBeam and Sonar.
 * Runtime only: accepted damage kind and liquid flags are immutable through
 * flight. Laser/TriBeam profiles retain their neutral zero liquid flags.
 * blast_radius/kind are copied at acceptance, never resolved from a mutable
 * shooter. Zero-initialized direct-only families do not request a blast. */
typedef struct scene_ai_laser_projectile {
    rf_weapon_flight flight;rf_weapon_flight_liquid_state liquid;
    uint32_t source,liquid_weapon_flags;int32_t weapon,damage_kind;float damage,basis[9];
    float blast_radius;int32_t blast_kind;
} scene_ai_laser_projectile;
static scene_ai_laser_profile scene_ai_laser_definition;

/* Bounded unsigned authored decimal syntax. NXDK strtod/strtof are stubs. */
static int scene_ai_laser_number(const char **cursor,float *out)
{
    const char *p=*cursor;double value=0,scale=1;uint32_t digits=0;
    while(*p==' ' || *p=='\t')++p;
    while(*p>='0' && *p<='9'){
        if(++digits>9)return RF_FORMAT;
        value=value*10+(*p++-'0');
    }
    if(*p=='.'){++p;while(*p>='0' && *p<='9'){
        if(++digits>12)return RF_FORMAT;
        scale*=.1;value+=(*p++-'0')*scale;
    }}
    if(!digits || value>1000000)return RF_FORMAT;
    *cursor=p;*out=(float)value;return RF_OK;
}
static int scene_ai_laser_numbers(const char *p,float *a,float *b)
{
    int status=scene_ai_laser_number(&p,a);if(status)return status;
    if(b){status=scene_ai_laser_number(&p,b);if(status)return status;}
    while(*p==' ' || *p=='\t')++p;
    return *p?RF_FORMAT:RF_OK;
}
static int scene_ai_laser_profile_read(const void *bytes,uint32_t length,scene_ai_laser_profile *out)
{
    const unsigned char *text=bytes;scene_ai_laser_profile value={0};
    uint32_t at=0,mask=0;int selected=0;
    if(!text || !out)return RF_RANGE;
    value.primary.burst_count=value.primary.projectiles=1;
    value.primary.ai_damage_scale[0]=value.primary.ai_damage_scale[1]=1;
    value.primary.damage_kind=1;value.weapon=-1;
    while(at<length){
        char line[512],*p,*end,*v;uint32_t n=0,bit=0;int quote=0,status;float a,b;
        while(at<length && text[at]!='\r' && text[at]!='\n'){
            if(n+1>=sizeof(line))return RF_RANGE;
            line[n++]=(char)text[at++];
        }
        while(at<length && (text[at]=='\r' || text[at]=='\n'))++at;
        line[n]=0;
        for(p=line;*p;p++){
            if(*p=='"')quote=!quote;
            if(!quote && p[0]=='/' && p[1]=='/'){*p=0;break;}
        }
        p=line;while(*p==' ' || *p=='\t')++p;
        end=p+strlen(p);while(end>p && (end[-1]==' ' || end[-1]=='\t'))*--end=0;
        if(!strncmp(p,"$Name:",6)){
            if(selected)break;
            v=p+6;while(*v==' ' || *v=='\t')++v;
            selected=!strcmp(v,"\"Laser\"");continue;
        }
        if(!selected)continue;
        if(!strcmp(p,"#End"))break;
        v=strchr(p,':');
        if(!v)continue;
        *v++=0;
        while(*v==' ' || *v=='\t')++v;
        if(!strcmp(p,"$V3D Filename")){bit=1;if(strcmp(v,"\"laser01.vfx\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Ammo Type")){bit=2;if(strcmp(v,"\"power cell\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Homing")){bit=4;if(strcmp(v,"false"))return RF_FORMAT;}
        else if(!strcmp(p,"$Sticky")){bit=8;if(strcmp(v,"false"))return RF_FORMAT;}
        else if(!strcmp(p,"$Glow")){bit=16;if(strcmp(v,"false"))return RF_FORMAT;}
        else if(!strcmp(p,"$Velocity")){bit=32;status=scene_ai_laser_numbers(v,&value.speed,NULL);if(status)return status;}
        else if(!strcmp(p,"$Lifetime")){bit=64;status=scene_ai_laser_numbers(v,&value.lifetime,NULL);if(status)return status;}
        else if(!strcmp(p,"$Collision Radius")){bit=128;status=scene_ai_laser_numbers(v,&value.radius,NULL);if(status)return status;}
        else if(!strcmp(p,"$Damage Radius")){bit=256;status=scene_ai_laser_numbers(v,&a,NULL);if(status || a!=0)return RF_FORMAT;}
        else if(!strcmp(p,"$Fire Wait")){bit=512;status=scene_ai_laser_numbers(v,&value.primary.fire_seconds,NULL);if(status)return status;}
        else if(!strcmp(p,"$Damage")){bit=1024;status=scene_ai_laser_numbers(v,&value.primary.damage,NULL);if(status)return status;}
        else if(!strcmp(p,"$AI attack range")){bit=2048;status=scene_ai_laser_numbers(v,&value.primary.ai_attack_range,&b);if(status || b!=5)return RF_FORMAT;}
        else if(!strcmp(p,"$Spread Degrees")){bit=4096;status=scene_ai_laser_numbers(v,&value.primary.spread_degrees,NULL);if(status)return status;}
        else if(!strcmp(p,"$Damage Type")){bit=8192;if(strcmp(v,"\"bullet\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Clip Size") || !strcmp(p,"$Clip Reload Time") ||
            !strcmp(p,"$Impact Delay") || !strcmp(p,"$Alt Impact Delay") ||
            !strcmp(p,"$Num Projectiles") || !strcmp(p,"$AI Spread Degrees") ||
            !strcmp(p,"$AI Damage Scale") || !strcmp(p,"$Burst Mode") ||
            !strcmp(p,"$Piercing") || !strcmp(p,"$Weapon Type"))return RF_FORMAT;
        if(mask&bit)return RF_FORMAT;
        mask|=bit;
    }
    if(!selected)return RF_NOT_FOUND;
    if(mask!=16383)return RF_FORMAT;
    if(value.speed!=20.f || value.lifetime!=20.f || value.radius!=.2f ||
       value.primary.fire_seconds!=.4f || value.primary.damage!=80.f ||
       value.primary.ai_attack_range!=5.f || value.primary.spread_degrees!=2.5f)return RF_FORMAT;
    /* Preserve existing NPC default AI-spread policy (no authored AI override),
     * independently of undeviating. That flag is NOT a spread-zero proof. */
    value.primary.alt_fire_seconds=value.primary.fire_seconds;
    value.primary.alt_damage=value.primary.damage;
    value.primary.alt_spread_degrees=value.primary.spread_degrees;
    *out=value;return RF_OK;
}
static int campaign_enemy_laser_open(rf_vpp *tables,const rf_weapon_supply_catalog *supply,
    const rf_weapon_reset_catalog *flags)
{
    scene_ai_laser_profile next;rf_vpp_entry entry;void *text;int32_t weapon;int status;
    memset(&scene_ai_laser_definition,0,sizeof(scene_ai_laser_definition));scene_ai_laser_definition.weapon=-1;
    if(!tables || !supply || !flags || supply->names.count>64 ||
       flags->names.count!=supply->names.count)return RF_RANGE;
    weapon=rf_weapon_name_find(&supply->names,"Laser");
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.primary_count ||
       rf_weapon_name_find(&flags->names,"Laser")!=weapon)return RF_FORMAT;
    if(supply->definitions[weapon].ammo_type<0 || supply->definitions[weapon].ammo_type>=32 ||
       supply->definitions[weapon].magazine!=0 || supply->definitions[weapon].capacity!=200 ||
       flags->definitions[weapon].flags_264!=0 || flags->definitions[weapon].flags_268!=0x28u)return RF_FORMAT;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>128u*1024u)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=scene_ai_laser_profile_read(text,entry.size,&next);
    free(text);if(status)return status;
    next.weapon=weapon;next.ready=1;scene_ai_laser_definition=next;return RF_OK;
}
static uint32_t campaign_enemy_laser_weapon(int32_t weapon)
{return scene_ai_laser_definition.ready && weapon==scene_ai_laser_definition.weapon;}
static int campaign_enemy_laser_select(const campaign_npc_body *owner,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection next={0};int32_t weapon;
    if(!owner || !supply || !out || supply->names.count>64)return RF_RANGE;
    weapon=owner->view.weapons[0];if(!campaign_enemy_laser_weapon(weapon))return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.count || !owner->inventory.owned[weapon])return RF_NOT_FOUND;
    next.primary=&scene_ai_laser_definition.primary;next.ammo=supply->definitions+weapon;
    if(next.ammo->ammo_type<0 || next.ammo->ammo_type>=32 || next.ammo->magazine || next.ammo->capacity!=200)return RF_FORMAT;
    next.slot=UINT32_MAX;next.reserve_fed=1;*out=next;return RF_OK;
}
static int campaign_enemy_laser_ready(const rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *ammo,int32_t weapon,uint32_t *reload_due,
    int32_t *reload_weapon,uint32_t *ready,uint32_t *event)
{
    if(!inventory || !ammo || !reload_due || !reload_weapon || !ready || !event ||
       !campaign_enemy_laser_weapon(weapon) || weapon<0 || weapon>=64 || !inventory->owned[weapon] ||
       ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine || ammo->capacity!=200 ||
       inventory->loaded[weapon] || inventory->reserve[ammo->ammo_type]<0)return RF_RANGE;
    *reload_due=0;*reload_weapon=-1;*ready=inventory->reserve[ammo->ammo_type]>0;*event=*ready?0:3;return RF_OK;
}
#endif
