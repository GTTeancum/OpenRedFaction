#ifndef RF_SCENE_VEHICLE_PROFILE_RESOURCES_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_RESOURCES_INCLUDED
/* Active profile loading extracted from scene_miner startup, with unchanged
 * budgets, definitions and fresh-owner ammunition policy. This owns only the
 * supplied stream's vehicle resources; it neither selects an authored UID nor
 * publishes a vehicle runtime or registry identity. Profiles: Driller1, APC2,
 * Jeep3, submarine4, Fighter5. No cross-class handoff is enabled by this file.
 *
 * Caller assigns driller_position/basis beforehand (authored or DEV policy),
 * supplies an empty resource owner, and closes partially opened resources via
 * normal scene teardown if any step fails. Archives are borrowed, never closed.
 * Include after vehicle resource/weapon/damage helper definitions. */
static int scene_vehicle_profile_resources_open(scene_stream *stream,uint32_t profile,
    const char *tables_path,rf_vpp *tables,rf_vpp *archive,rf_vpp *maps,uint32_t map_count)
{
    int status=RF_OK;
    if(!stream||!tables_path||!tables||!archive||(!maps&&map_count)||profile<1||profile>5)return RF_RANGE;
    if(stream->driller||stream->driller_runtime||stream->driller_bits||stream->driller_cockpit||
       stream->jeep_gun||stream->apc_mortar||stream->submarine_torpedo)return RF_RANGE;
    if(profile>=2)status=scene_vehicle_resources_open(tables_path,profile==5?"Fighter01":profile==4?"sub":profile==3?"Jeep01":"APC","interface_1",archive,maps,map_count,2*1024*1024,&stream->driller);
    else status=scene_driller_resources_open(tables_path,archive,maps,map_count,1024*1024,&stream->driller);
    if(status)printf("VEHICLE_RESOURCE_FAIL chassis %d\n",status);
    if(!status && profile==1)status=scene_driller_bit_animation_open(&stream->driller->tags,archive,maps,map_count,512*1024,&stream->driller_bits);
    if(!status)status=profile>=2?scene_vehicle_cockpit_open(profile==5?"fighter01.vfx":profile==4?"sub.vfx":profile==3?"jeep.vfx":"APC.vfx",archive,maps,map_count,2*1024*1024,&stream->driller_cockpit):scene_driller_cockpit_open(archive,maps,map_count,2*1024*1024,&stream->driller_cockpit);
    if(status)printf("VEHICLE_RESOURCE_FAIL cockpit %d\n",status);
    if(!status && profile==5){
        status=scene_fighter_weapon_open(tables,&stream->driller->tags,"muzzle_1","secondary_1",512*1024,&stream->fighter_weapon);
        if(!status){stream->fighter_weapon.primary_reserve=(int32_t)stream->fighter_weapon.primary.capacity;
            stream->fighter_weapon.rocket_reserve=(int32_t)stream->fighter_weapon.rocket.capacity;}
    }
    if(!status && profile==2){
        status=scene_vehicle_primary_apc_open(tables,&stream->driller->tags,512*1024,
            &stream->apc_primary.definition,&stream->apc_primary.scheduler,&stream->apc_primary.muzzle);
        stream->apc_primary.reserve=(int32_t)stream->apc_primary.definition.capacity;
        stream->apc_primary.random.value=1;memset(rf_scene_apc_primary,0,sizeof(rf_scene_apc_primary));
        if(!status)status=scene_apc_secondary_open(tables,&stream->driller->tags,512*1024,&stream->apc_secondary);
        stream->apc_secondary.reserve=(int32_t)stream->apc_secondary.definition.capacity;
        memset(rf_scene_apc_secondary,0,sizeof(rf_scene_apc_secondary));
        if(!status)status=scene_vehicle_aim_limits_load(tables,"APC",512*1024,&stream->apc_aim_limits);
        if(!status)status=scene_apc_secondary_visual_open(archive,maps,map_count,512*1024,&stream->apc_mortar);
    }
    if(!status && profile==4){
        status=scene_submarine_weapon_open(tables,&stream->driller->tags,"primary_1",512*1024,&stream->submarine_weapon);
        /* Explicit DEV supply, never implicit authored ownership. */
        if(!status)stream->submarine_weapon.reserve=(int32_t)stream->submarine_weapon.definition.capacity;
        if(!status)status=scene_submarine_weapon_model_open(archive,maps,map_count,512*1024,&stream->submarine_torpedo);
    }
    if(!status && profile==1)status=scene_driller_weapon_open(tables,&stream->driller->tags,512*1024,&stream->driller_weapon);
    if(!status && profile==3){
        status=scene_jeep_gun_resources_open(archive,maps,map_count,&stream->driller->tags,576*1024,&stream->jeep_gun);
        if(!status)status=scene_vehicle_primary_load(tables,"Jeep Gun",512*1024,&stream->apc_primary.definition);
        if(!status)stream->apc_primary.muzzle=stream->jeep_gun->muzzle;
        stream->apc_primary.reserve=(int32_t)stream->apc_primary.definition.capacity;
        stream->apc_primary.random.value=1;memset(rf_scene_apc_primary,0,sizeof(rf_scene_apc_primary));
        if(!status)status=scene_vehicle_aim_limits_load(tables,"Jeep01",512*1024,&stream->apc_aim_limits);
    }
    if(!status)status=profile>=2?scene_vehicle_damage_open(profile==5?"Fighter01":profile==4?"sub":profile==3?"Jeep01":"APC",&stream->driller_damage_prototype,tables,stream->driller,0,0,0,512*1024):scene_driller_damage_open(&stream->driller_damage_prototype,tables,stream->driller,0,0,0,512*1024);
    if(!status){float seat[12];status=scene_driller_seat_pose(stream->driller,stream->driller_position,stream->driller_basis,seat);}
    return status;
}
#endif
