#ifndef RF_SCENE_VEHICLE_PROFILE_PRELOAD_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_PRELOAD_INCLUDED
/* Additional ground profiles are optional until a transactional handoff is
 * admitted. No allocation failure here may invalidate the current vehicle.
 * Masks use bit(profile), with profiles1..3. Snapshot is preserved at teardown.
 * demand,loaded,merged,resident bytes,added slots,added bytes,status,free pages */
uint32_t rf_scene_vehicle_profile_packs[16];
#ifdef RF_IMAGE_XBOX_NATIVE
extern uint32_t rf_xbox_available_memory_pages(void);
#endif
static void scene_vehicle_profile_preload(scene_stream *s,const char *tables_path,
    rf_vpp *archive,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp tables={0};uint32_t mask=0,remaining=4u*1024u*1024u;int status;
    memset(rf_scene_vehicle_profile_packs,0,sizeof(rf_scene_vehicle_profile_packs));
    if(!s||!s->driller||rf_scene_vehicle_enabled<1||rf_scene_vehicle_enabled>3)return;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *p=campaign_passive_vehicles+i;
        uint32_t profile=p->resource_kind==3?1:p->resource_kind==4?2:p->resource_kind==5?3:0;
        uint32_t source;
        if(!profile||profile==rf_scene_vehicle_enabled||p->group_owned||p->attached||
           p->damage.destroyed||p->damage.state.effects.health<=0||
           (p->damage.object_flags&(2u|0x4000u)))continue;
        if(scene_vehicle_switch_seed(p->uid,&source))continue;
        if(campaign_seeds.items[source].spawn.seat_host_uid!=-1||
           (campaign_seeds.items[source].spawn.creation_flags&2u))continue;
        mask|=1u<<profile;
    }
    mask|=scene_vehicle_switch_boot_profile_mask()&~(1u<<rf_scene_vehicle_enabled);
    rf_scene_vehicle_profile_packs[0]=mask;if(!mask)return;
#ifdef RF_IMAGE_XBOX_NATIVE
    /* Conservative admission policy, not a measured contiguous-GPU guarantee:
     * retain16MiB for remaining lighting, render uploads/cache and gameplay.
     * The4MiB pack budget also bounds sequential loader scratch. */
    uint32_t pages=rf_xbox_available_memory_pages();rf_scene_vehicle_profile_packs[7]=pages;
    if(pages<=4096u){rf_scene_vehicle_profile_packs[6]=RF_RANGE;return;}
    if((pages-4096u)*4096u<remaining)remaining=(pages-4096u)*4096u;
#endif
    status=rf_vpp_open(&tables,tables_path);
    for(uint32_t profile=1;!status&&profile<=3;profile++)if(mask&(1u<<profile)){
        uint32_t kind=profile==1?3:profile==2?4:5;
        status=scene_vehicle_profile_pack_open(profile,s->passive_vehicle_resources[kind],
            &tables,archive,maps,map_count,scene_gravity.acceleration,remaining,&s->vehicle_profile_packs[profile]);
        if(!status&&profile==1){
            if(!s->driller_runtime)status=RF_NOT_FOUND;
#ifdef RF_IMAGE_XBOX_NATIVE
            if(!status)status=rf_geomod_template_load("D:\\driller-single.bin",s->driller_runtime->shapes);
            if(!status)status=rf_geomod_template_load("D:\\driller-double.bin",s->driller_runtime->shapes+1);
#else
            if(!status)status=rf_geomod_template_load("build/data/driller-single.bin",s->driller_runtime->shapes);
            if(!status)status=rf_geomod_template_load("build/data/driller-double.bin",s->driller_runtime->shapes+1);
#endif
            if(status)scene_vehicle_profile_pack_close(&s->vehicle_profile_packs[profile]);
        }
        if(!status){
            uint32_t bytes=s->vehicle_profile_packs[profile]->resident_bytes;
            remaining-=bytes;rf_scene_vehicle_profile_packs[3]+=bytes;
            rf_scene_vehicle_profile_packs[1]|=1u<<profile;
        }
    }
    rf_vpp_close(&tables);rf_scene_vehicle_profile_packs[6]=(uint32_t)status;
}
static void scene_vehicle_profile_preload_merge(scene_stream *s,rf_materials *materials,uint32_t budget)
{
    if(!s||!materials)return;
    for(uint32_t profile=1;profile<=3;profile++)if(s->vehicle_profile_packs[profile]){
        uint32_t slots=materials->count,bytes=materials->allocated_bytes;
        uint32_t resident=s->vehicle_profile_packs[profile]->resident_bytes;
        int status=scene_vehicle_profile_pack_materials_merge(s->vehicle_profile_packs[profile],
            materials,RF_CAMPAIGN_TEXTURE_SLOTS,budget);
        if(status){
            rf_scene_vehicle_profile_packs[6]=(uint32_t)status;
            rf_scene_vehicle_profile_packs[3]-=s->vehicle_profile_packs[profile]->resident_bytes;
            rf_scene_vehicle_profile_packs[1]&=~(1u<<profile);
            scene_vehicle_profile_pack_close(&s->vehicle_profile_packs[profile]);
        }else{
            rf_scene_vehicle_profile_packs[3]-=resident-s->vehicle_profile_packs[profile]->resident_bytes;
            rf_scene_vehicle_profile_packs[2]|=1u<<profile;
            rf_scene_vehicle_profile_packs[4]+=materials->count-slots;
            rf_scene_vehicle_profile_packs[5]+=materials->allocated_bytes-bytes;
        }
    }
    if(rf_scene_vehicle_profile_packs[2]){
        uint32_t profile=rf_scene_vehicle_enabled,kind=profile+2;
        int status=s->passive_vehicle_sphere_count[kind]?RF_OK:scene_passive_vehicle_collision_open(s,kind,s->driller);
        if(!status)status=scene_vehicle_profile_pack_adopt(s,profile,materials,
            scene_gravity.acceleration,&s->vehicle_profile_packs[profile]);
        rf_scene_vehicle_profile_packs[8]=(uint32_t)status;
        if(status)rf_scene_vehicle_profile_packs[6]=(uint32_t)status;
    }
}
#endif
