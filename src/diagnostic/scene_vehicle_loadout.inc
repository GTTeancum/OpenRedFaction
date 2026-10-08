#ifndef RF_SCENE_VEHICLE_LOADOUT_INCLUDED
#define RF_SCENE_VEHICLE_LOADOUT_INCLUDED
/* Original class defaults41bf57..41c073, then instance overrides464010.
 * Empty/unknown override names inherit; explicit none clears that category.
 * Loaded class defaults and catalog names are authoritative, not a profile's
 * hardware capacity. L13S3 Fighter8955 is deliberately none/none. */
static const char *scene_vehicle_loadout_class(uint32_t profile)
{
    static const char *const classes[]={"","Driller01","APC","Jeep01","sub","Fighter01","masako_fighter"};
    return profile<7?classes[profile]:"";
}
static const char *scene_vehicle_loadout_weapon(uint32_t profile,uint32_t side)
{
    static const char *const primary[]={"","Drill","APC Minigun","Jeep Gun","Torpedo","Fighter Minigun","Fighter Minigun"};
    static const char *const secondary[]={"","","APC Rocket","","","Fighter Rocket","Fighter Rocket"};
    return profile<7?(side?secondary[profile]:primary[profile]):"";
}
/* Outputs retain the selected IDs even if their class-specific firing adapter
 * is unsupported. Such a weapon must not silently become a different gun. */
static int scene_vehicle_loadout_resolve(uint32_t uid,uint32_t profile,int32_t weapons[2],uint32_t *mask)
{
    int32_t value[2]={-1,-1};uint32_t admitted=0,found=0,slot=0;
    if(!weapons||!mask||profile<1||profile>6)return RF_RANGE;
    if(!uid){ /* Explicit enemy-free developer supply, no authored instance. */
        for(uint32_t side=0;side<2;side++)if(*scene_vehicle_loadout_weapon(profile,side)){
            value[side]=rf_weapon_name_find(&campaign_weapon_supply.names,scene_vehicle_loadout_weapon(profile,side));admitted|=1u<<side;
        }
        memcpy(weapons,value,sizeof(value));*mask=admitted;return RF_OK;
    }
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){slot=i;++found;}
    if(found!=1||!campaign_seeds.items||!campaign_base_motions.classes)return RF_NOT_FOUND;
    const rf_level_entity *record=&campaign_seeds.records.items[slot].record;
    uint32_t cls=campaign_seeds.items[slot].class_index;
    if(strcmp(record->class_name,scene_vehicle_loadout_class(profile))||cls>=campaign_base_motions.class_count)return RF_FORMAT;
    const rf_entity_default_weapons *defaults=&campaign_base_motions.classes[cls].default_weapons;
    const int32_t initial[2]={defaults->primary,defaults->secondary};
    const char *overrides[2]={record->primary_weapon,record->secondary_weapon};
    for(uint32_t side=0;side<2;side++){
        if(initial[side]>=0){
            if((uint32_t)initial[side]>=campaign_base_motions.weapons.count)return RF_FORMAT;
            value[side]=rf_weapon_name_find(&campaign_weapon_supply.names,campaign_base_motions.weapons.names[initial[side]]);
            if(value[side]<0)return RF_FORMAT;
        }
        if(!memchr(overrides[side],0,RF_LEVEL_NAME_CAPACITY))return RF_FORMAT;
        if(scene_npc_loadout_equal(overrides[side],"none"))value[side]=-1;
        else{
            int32_t override=rf_weapon_name_find(&campaign_weapon_supply.names,overrides[side]);
            if(override>=0)value[side]=override;
        }
        const char *supported=scene_vehicle_loadout_weapon(profile,side);
        if(value[side]>=0&&*supported&&value[side]==rf_weapon_name_find(&campaign_weapon_supply.names,supported))admitted|=1u<<side;
    }
    memcpy(weapons,value,sizeof(value));*mask=admitted;return RF_OK;
}
static uint32_t scene_vehicle_loadout_selected_mask(void)
{
    int32_t weapons[2];uint32_t mask=0;
    return scene_vehicle_loadout_resolve((uint32_t)campaign_authored_vehicle_uid,rf_scene_vehicle_enabled,weapons,&mask)?0:mask;
}
/* None clears selected/owned weapons, not dormant ammunition:464010's
 * inventory ownership-byte clear leaves reserves intact. Keep those finite
 * rounds through load/switch; fire/HUD admission uses the selected IDs/mask.
 * Pending trigger/spin-up cannot remain armed for a removed weapon. Existing
 * flights and their class definitions still belong to their launch owner. */
static void scene_vehicle_loadout_normalize(scene_stream *s,uint32_t profile,const int32_t weapons[2],uint32_t mask,rf_entity_view *view)
{
    if(view){view->weapons[0]=weapons[0];view->weapons[1]=weapons[1];}
    if(profile==5){
        if(!(mask&1u)){s->fighter_weapon.primary_fire.held=0;s->fighter_weapon.primary_fire.warmup=0;}
        if(!(mask&2u)){s->fighter_weapon.rocket_fire.held=0;s->fighter_weapon.rocket_fire.warmup=0;}
    }else if(profile==2||profile==3){
        if(!(mask&1u)){s->apc_primary.scheduler.held=0;s->apc_primary.scheduler.warmup=0;}
    }
}
static int scene_vehicle_loadout_apply(scene_stream *s,uint32_t profile,uint32_t uid,rf_entity_view *view)
{
    int32_t weapons[2];uint32_t mask;int status=scene_vehicle_loadout_resolve(uid,profile,weapons,&mask);
    if(status)return status;scene_vehicle_loadout_normalize(s,profile,weapons,mask,view);return RF_OK;
}
#endif
