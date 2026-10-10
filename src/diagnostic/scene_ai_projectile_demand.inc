#ifndef RF_SCENE_AI_PROJECTILE_DEMAND_INC
#define RF_SCENE_AI_PROJECTILE_DEMAND_INC
/* Include after scene_precision_drop_demand.inc and the campaign scene globals.
 * Borrow the current constructed NPC loadout before campaign_actors_restore
 * binds persistent slots. Bits4/5 mean shared rocket/grenade resources, NOT
 * player ownership or first-person-view demand. No allocation or mutation. */
static uint32_t scene_ai_projectile_resource_mask(uint32_t *tankbot_missile,uint32_t *drone_missile)
{
    int32_t rocket,grenade,tankbot,drone;uint32_t i,mask=0;
    if(!tankbot_missile || !drone_missile)return 0;
    *tankbot_missile=*drone_missile=0;
    if(!campaign_npc_bodies || campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS ||
       !campaign_weapon_supply.names.count || campaign_weapon_supply.names.count>64 ||
       rf_scene_defeated_actors.count>RF_CAMPAIGN_ACTOR_SLOTS ||
       rf_scene_defeated_actors.level_count>RF_CAMPAIGN_PICKUP_LEVELS)return 0;
    rocket=rf_weapon_name_find(&campaign_weapon_supply.names,"Rocket Launcher");
    grenade=rf_weapon_name_find(&campaign_weapon_supply.names,"Grenade");
    tankbot=rf_weapon_name_find(&campaign_weapon_supply.names,"Tankbot Missile");
    drone=rf_weapon_name_find(&campaign_weapon_supply.names,"Drone Missile");
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *owner=campaign_npc_bodies+i;
        int32_t held=owner->view.weapons[0];uint32_t slot;
        if(!owner->registration.view || (owner->object_flags&2u) || (owner->view.flags_810&1u) ||
           owner->damage.effects.health<=0)continue;
        if(owner->persistence_registered && owner->persistence_slot>=rf_scene_defeated_actors.count)continue;
        if(scene_precision_owner_persistence_slot(owner,&slot) && rf_scene_defeated_actors.items[slot].retired)continue;
        /* Hidden live actors can be revealed by events. Empty ammunition is
         * gameplay state, not proof the authored weapon will never be used. */
        if(held>=0 && (uint32_t)held<campaign_weapon_supply.names.count && owner->inventory.owned[held]){
            if(rocket>=0 && held==rocket)mask|=1u<<4;
            if(grenade>=0 && held==grenade)mask|=1u<<5;
        }
        if(tankbot>=0 && owner->view.weapons[1]==tankbot && owner->inventory.owned[tankbot]){
            mask|=1u<<4;*tankbot_missile=1;
        }
        if(drone>=0 && owner->view.weapons[1]==drone && owner->inventory.owned[drone]){
            mask|=1u<<4;*drone_missile=1;
        }
    }
    return mask;
}
/* The shared primary/explosive readers intentionally do not retain homing,
 * sticky, ammo-name or projectile-model declarations. Admit this exact known
 * straight-flight profile once at startup, alongside those shared readers. */
static int scene_ai_drone_missile_profile_read(const void *bytes,uint32_t length)
{
    const unsigned char *text=bytes;uint32_t at=0,mask=0;int selected=0;
    if(!text)return RF_RANGE;
    while(at<length){
        char line[512],*p,*end,*value;uint32_t n=0,bit=0;int quoted=0;
        while(at<length && text[at]!='\r' && text[at]!='\n'){
            if(n+1>=sizeof(line))return RF_RANGE;line[n++]=(char)text[at++];
        }
        while(at<length && (text[at]=='\r' || text[at]=='\n'))++at;
        line[n]=0;
        for(p=line;*p;p++){
            if(*p=='"')quoted=!quoted;
            if(!quoted && p[0]=='/' && p[1]=='/'){*p=0;break;}
        }
        p=line;while(*p==' ' || *p=='\t')++p;
        end=p+strlen(p);while(end>p && (end[-1]==' ' || end[-1]=='\t'))*--end=0;
        if(!strncmp(p,"$Name:",6)){
            if(selected)break;
            value=p+6;while(*value==' ' || *value=='\t')++value;
            if(!strcmp(value,"\"Drone Missile\""))selected=1;
            continue;
        }
        if(!selected)continue;
        if(!strcmp(p,"#End"))break;
        value=strchr(p,':');if(!value)continue;*value++=0;
        while(*value==' ' || *value=='\t')++value;
        if(!strcmp(p,"$V3D Filename")){bit=1;if(strcmp(value,"\"DrillMissile01.VFX\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Ammo Type")){bit=2;if(strcmp(value,"\"15cm rocket\""))return RF_FORMAT;}
        else if(!strcmp(p,"$Homing")){bit=4;if(strcmp(value,"false"))return RF_FORMAT;}
        else if(!strcmp(p,"$Sticky")){bit=8;if(strcmp(value,"false"))return RF_FORMAT;}
        if(mask&bit)return RF_FORMAT;mask|=bit;
    }
    return !selected?RF_NOT_FOUND:mask==15?RF_OK:RF_FORMAT;
}
static int scene_ai_drone_missile_resources_open(rf_vpp *tables)
{
    rf_weapon_explosive_definition flight;rf_weapon_primary_definition primary;
    const rf_weapon_acquire_definition *ammo;const rf_weapon_descriptor *flags;
    rf_vpp_entry entry;void *text;int32_t weapon;int status;
    if(!tables || !campaign_weapon_supply.names.count || campaign_weapon_supply.names.count>64 ||
       campaign_weapon_reset.names.count!=campaign_weapon_supply.names.count)return RF_RANGE;
    weapon=rf_weapon_name_find(&campaign_weapon_supply.names,"Drone Missile");
    if(weapon<0)return RF_NOT_FOUND;
    if(weapon>=64 || (uint32_t)weapon>=campaign_weapon_supply.names.count ||
       rf_weapon_name_find(&campaign_weapon_reset.names,"Drone Missile")!=weapon)return RF_FORMAT;
    ammo=campaign_weapon_supply.definitions+weapon;flags=campaign_weapon_reset.definitions+weapon;
    if(ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine!=0 || ammo->capacity!=12 ||
       flags->flags_264!=0x18u || flags->flags_268!=0x28u)return RF_FORMAT;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>128u*1024u)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=scene_ai_drone_missile_profile_read(text,entry.size);
    if(!status)status=rf_weapon_explosive_read(text,entry.size,"Drone Missile",&flight);
    if(!status)status=rf_weapon_primary_read(text,entry.size,"Drone Missile",&primary);
    free(text);if(status)return status;
    if(flight.speed!=20.f || flight.lifetime!=5.f || flight.collision_radius!=.15f ||
       flight.damage_radius!=2.f || flight.crater_radius!=5.f || flight.glow ||
       primary.magazine || primary.reload_seconds!=0 || primary.projectiles!=1 ||
       primary.damage_kind!=3 || primary.damage!=15.f || primary.fire_seconds!=4.f ||
       primary.ai_attack_range!=30.f || primary.burst_count!=1 || primary.burst_seconds!=0 ||
       primary.piercing_enabled || primary.ai_damage_scale[0]!=1.f || primary.ai_damage_scale[1]!=1.f)return RF_FORMAT;
    campaign_drone_missile=flight;campaign_drone_missile_primary=primary;return RF_OK;
}
#endif
