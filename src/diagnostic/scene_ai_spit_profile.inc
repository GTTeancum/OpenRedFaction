#ifndef RF_SCENE_AI_SPIT_PROFILE_INCLUDED
#define RF_SCENE_AI_SPIT_PROFILE_INCLUDED
/* Exact installed clipless, finite, delayed AP primaries. Include after
 * scene_ai_laser_profile.inc for bounded decimals and direct-flight shape.
 * Do not widen Laser or conventional/explosive admission to make it fit. */
typedef struct scene_ai_spit_profile {
    scene_ai_laser_profile flight;int32_t delay_ms,blast_kind;float blast_radius;
    const char *owner_class;
} scene_ai_spit_profile;
/* Independently grounded installed rows, not a Big alias for Small. The
 * matching VFX/flight/AP/supply values really occur in both original rows.
 * Big radial kind3 comes from4c62da..4c62f5, not its direct AP descriptor. */
typedef struct scene_ai_spit_contract {
    const char *name,*quoted_name,*owner_class,*launch;
    float speed,lifetime,radius,damage,range,cadence,delay,blast_radius;
    int32_t capacity,blast_kind;
} scene_ai_spit_contract;
static const scene_ai_spit_contract scene_ai_spit_contracts[2]={
    {"Rock Snake Spit","\"Rock Snake Spit\"","Rock Snake","\"RSnake Spit\"",
     30.f,10.f,.5f,80.f,50.f,3.f,1.f,0.f,200,0},
    {"Big Rock Snake Spit","\"Big Rock Snake Spit\"","Big Snake","\"BSnake Spit\"",
     30.f,10.f,.5f,80.f,50.f,2.5f,.8f,3.f,200,3}
};
static scene_ai_spit_profile scene_ai_spit_definitions[2];
static int scene_ai_spit_profile_read(const void *bytes,uint32_t length,
    const scene_ai_spit_contract *expected,scene_ai_spit_profile *out)
{
    const unsigned char *text=bytes;scene_ai_spit_profile value={0};
    uint32_t at=0,mask=0;int selected=0;
    if(!text || !expected || !out)return RF_RANGE;
    value.flight.primary.burst_count=value.flight.primary.projectiles=1;
    value.flight.primary.ai_damage_scale[0]=value.flight.primary.ai_damage_scale[1]=1;
    value.flight.primary.damage_kind=2;value.flight.weapon=-1;
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
            selected=!strcmp(v,expected->quoted_name);continue;
        }
        if(!selected)continue;
        if(!strcmp(p,"#End"))break;
        v=strchr(p,':');
        if(!v)continue;
        *v++=0;
        while(*v==' ' || *v=='\t')++v;
        if(!strcmp(p,"$V3D Filename")){bit=1;if(strcmp(v,"\"SpitAttack.vfx\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Ammo Type")){bit=2;if(strcmp(v,"\"12mm\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Homing")){bit=4;if(strcmp(v,"false"))return RF_FORMAT;}
        else if(!strcmp(p,"$Sticky")){bit=8;if(strcmp(v,"false"))return RF_FORMAT;}
        else if(!strcmp(p,"$Glow")){bit=16;if(strcmp(v,"false"))return RF_FORMAT;}
        else if(!strcmp(p,"$Velocity")){bit=32;status=scene_ai_laser_numbers(v,&value.flight.speed,NULL);if(status)return status;}
        else if(!strcmp(p,"$Lifetime")){bit=64;status=scene_ai_laser_numbers(v,&value.flight.lifetime,NULL);if(status)return status;}
        else if(!strcmp(p,"$Collision Radius")){bit=128;status=scene_ai_laser_numbers(v,&value.flight.radius,NULL);if(status)return status;}
        else if(!strcmp(p,"$Damage Radius")){bit=256;status=scene_ai_laser_numbers(v,&value.blast_radius,NULL);if(status)return status;}
        else if(!strcmp(p,"$Fire Wait")){bit=512;status=scene_ai_laser_numbers(v,&value.flight.primary.fire_seconds,NULL);if(status)return status;}
        else if(!strcmp(p,"$Damage")){bit=1024;status=scene_ai_laser_numbers(v,&value.flight.primary.damage,NULL);if(status)return status;}
        else if(!strcmp(p,"$AI attack range")){bit=2048;status=scene_ai_laser_numbers(v,&value.flight.primary.ai_attack_range,&b);if(status || b!=expected->range)return RF_FORMAT;}
        else if(!strcmp(p,"$Impact Delay")){
            bit=4096;
            status=scene_ai_laser_numbers(v,&a,NULL);
            if(status || a!=expected->delay)return RF_FORMAT;
            value.delay_ms=(int32_t)(a*1000.f+.5f);
        }
        else if(!strcmp(p,"$Damage Type")){bit=8192;if(strcmp(v,"\"armor piercing bullet\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Launch")){bit=16384;if(strcmp(v,expected->launch))return RF_FORMAT;}
        else if(!strcmp(p,"$Clip Size") || !strcmp(p,"$Clip Reload Time") ||
            !strcmp(p,"$Alt Impact Delay") || !strcmp(p,"$Spread Degrees") ||
            !strcmp(p,"$Num Projectiles") || !strcmp(p,"$AI Spread Degrees") ||
            !strcmp(p,"$AI Damage Scale") || !strcmp(p,"$Burst Mode") ||
            !strcmp(p,"$Piercing") || !strcmp(p,"$Weapon Type"))return RF_FORMAT;
        if(mask&bit)return RF_FORMAT;
        mask|=bit;
    }
    if(!selected)return RF_NOT_FOUND;
    if(mask!=32767)return RF_FORMAT;
    if(value.flight.speed!=expected->speed || value.flight.lifetime!=expected->lifetime ||
       value.flight.radius!=expected->radius || value.flight.primary.fire_seconds!=expected->cadence ||
       value.flight.primary.damage!=expected->damage || value.flight.primary.ai_attack_range!=expected->range ||
       value.blast_radius!=expected->blast_radius)return RF_FORMAT;
    value.flight.primary.alt_fire_seconds=value.flight.primary.fire_seconds;
    value.flight.primary.alt_damage=value.flight.primary.damage;
    value.blast_kind=expected->blast_kind;value.owner_class=expected->owner_class;
    *out=value;return RF_OK;
}
static int campaign_enemy_spit_open(rf_vpp *tables,const rf_weapon_supply_catalog *supply,
    const rf_weapon_reset_catalog *flags)
{
    scene_ai_spit_profile next[2]={{0}};rf_vpp_entry entry;void *text;uint32_t i;int32_t weapon;int status;
    memset(scene_ai_spit_definitions,0,sizeof(scene_ai_spit_definitions));
    for(i=0;i<2;i++)scene_ai_spit_definitions[i].flight.weapon=-1;
    if(!tables || !supply || !flags || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count || flags->names.count!=supply->names.count ||
       flags->names.primary_count!=supply->names.primary_count)return RF_RANGE;
    for(i=0;i<2;i++){
        weapon=rf_weapon_name_find(&supply->names,scene_ai_spit_contracts[i].name);
        if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.primary_count ||
           rf_weapon_name_find(&flags->names,scene_ai_spit_contracts[i].name)!=weapon)return RF_FORMAT;
        /* Original weapon Flags2 table 0x5a2534: no_fire_through is bit 3;
         * bit 5 is undeviating, distinct from the entity drools-slime bit 5. */
        if(supply->definitions[weapon].ammo_type<0 || supply->definitions[weapon].ammo_type>=32 ||
           supply->definitions[weapon].magazine!=0 || supply->definitions[weapon].capacity!=scene_ai_spit_contracts[i].capacity ||
           flags->definitions[weapon].flags_264!=0 || flags->definitions[weapon].flags_268!=0x08u)return RF_FORMAT;
    }
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>128u*1024u)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    for(i=0;!status && i<2;i++){
        status=scene_ai_spit_profile_read(text,entry.size,scene_ai_spit_contracts+i,next+i);
        if(!status){next[i].flight.weapon=rf_weapon_name_find(&supply->names,scene_ai_spit_contracts[i].name);next[i].flight.ready=1;}
    }
    free(text);if(status)return status;
    memcpy(scene_ai_spit_definitions,next,sizeof(next));return RF_OK;
}
static const scene_ai_spit_profile *campaign_enemy_spit_profile(int32_t weapon)
{
    uint32_t i;
    for(i=0;i<2;i++)if(scene_ai_spit_definitions[i].flight.ready &&
       scene_ai_spit_definitions[i].flight.weapon==weapon)return scene_ai_spit_definitions+i;
    return NULL;
}
static uint32_t campaign_enemy_spit_weapon(int32_t weapon)
{return campaign_enemy_spit_profile(weapon)!=NULL;}
/* Pair each selected-owned weapon with its exact authored class and drools
 * flag. A Big/Small cross-selection cannot borrow the other's admission. */
static uint32_t campaign_enemy_spit_owner(const campaign_npc_body *owner)
{
    uint32_t slot,cls;const scene_ai_spit_profile *profile;
    if(!owner || !campaign_npc_bodies || !campaign_seeds.items || !campaign_seeds.classes ||
       !campaign_seeds.records.items)return 0;
    for(slot=0;slot<campaign_npc_body_count;slot++)if(campaign_npc_bodies+slot==owner)break;
    if(slot==campaign_npc_body_count || slot>=campaign_seeds.records.count)return 0;
    profile=campaign_enemy_spit_profile(owner->view.weapons[0]);if(!profile)return 0;
    cls=campaign_seeds.items[slot].class_index;
    return cls<campaign_seeds.class_count && (campaign_seeds.classes[cls].physics.flags2&0x20u) &&
        scene_npc_loadout_equal(campaign_seeds.records.items[slot].record.class_name,profile->owner_class);
}
static int campaign_enemy_spit_select(const campaign_npc_body *owner,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection next={0};int32_t weapon;const scene_ai_spit_profile *profile;
    if(!owner || !supply || !out || supply->names.count>64)return RF_RANGE;
    weapon=owner->view.weapons[0];profile=campaign_enemy_spit_profile(weapon);
    if(!profile || !campaign_enemy_spit_owner(owner))return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.count || !owner->inventory.owned[weapon])return RF_NOT_FOUND;
    next.primary=&profile->flight.primary;next.ammo=supply->definitions+weapon;
    if(next.ammo->ammo_type<0 || next.ammo->ammo_type>=32 || next.ammo->magazine || next.ammo->capacity!=200)return RF_FORMAT;
    next.slot=UINT32_MAX;next.reserve_fed=1;*out=next;return RF_OK;
}
static int campaign_enemy_spit_ready(const rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *ammo,int32_t weapon,uint32_t *reload_due,
    int32_t *reload_weapon,uint32_t *ready,uint32_t *event)
{
    if(!inventory || !ammo || !reload_due || !reload_weapon || !ready || !event ||
       !campaign_enemy_spit_weapon(weapon) || weapon<0 || weapon>=64 || !inventory->owned[weapon] ||
       ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine || ammo->capacity!=200 ||
       inventory->loaded[weapon] || inventory->reserve[ammo->ammo_type]<0)return RF_RANGE;
    *reload_due=0;*reload_weapon=-1;*ready=inventory->reserve[ammo->ammo_type]>0;*event=*ready?0:3;return RF_OK;
}
#endif
