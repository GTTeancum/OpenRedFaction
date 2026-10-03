#ifndef RF_SCENE_VEHICLE_PROFILE_PACK_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_PACK_INCLUDED
/* Include after runtime/profile_physics_prepare and auxiliary resource helpers.
 * One ground-class extras owner. Chassis is BORROWED and never modified/closed;
 * world registries, scene profile and renderer are never published here.
 * Geometry must remain allocated until scene retained-model teardown. */
struct scene_vehicle_profile_pack {
    const scene_driller_resources *chassis;
    scene_driller_cockpit *cockpit;scene_driller_bit_animation *bits;
    scene_jeep_gun_resources *gun;scene_driller_resources *mortar;
    uint32_t profile,merged,resident_bytes,peak_bytes,material_bytes;
    uint32_t bit_base,bit_textures,gun_base,gun_textures,mortar_base,mortar_textures;
    int32_t seat,gunner_seat,primary_muzzle,secondary_muzzle;
    scene_driller_physics physics;scene_driller_damage damage;rf_entity_view view;
    scene_driller_weapon drill;scene_vehicle_primary_definition primary;
    scene_vehicle_primary_state primary_initial;scene_apc_secondary_definition secondary;
    int32_t primary_reserve,secondary_reserve;rf_random_state primary_random;
    rf_entity_eye_limits aim_limits;
};
static void scene_vehicle_profile_pack_close(scene_vehicle_profile_pack **out)
{
    scene_vehicle_profile_pack *p;if(!out||!(p=*out))return;
    scene_driller_cockpit_close(&p->cockpit);scene_driller_bit_animation_close(&p->bits);
    scene_jeep_gun_resources_close(&p->gun);scene_driller_resources_close(&p->mortar);
    free(p);*out=NULL; /* Borrowed chassis and transferred scene pixels survive. */
}
static uint32_t scene_vehicle_profile_pack_cap(uint32_t budget,uint32_t used,uint32_t maximum)
{return used>=budget?0:(budget-used<maximum?budget-used:maximum);}
static void scene_vehicle_profile_pack_account(scene_vehicle_profile_pack *p,uint32_t resident,uint32_t peak)
{
    uint32_t demand=p->resident_bytes+peak;
    if(demand>p->peak_bytes)p->peak_bytes=demand;p->resident_bytes+=resident;
}
static int scene_vehicle_profile_pack_open(uint32_t profile,const scene_driller_resources *chassis,
    rf_vpp *tables,rf_vpp *archive,rf_vpp *maps,uint32_t map_count,float gravity,uint32_t budget,scene_vehicle_profile_pack **out)
{
    static const char *const classes[3]={"Driller01","APC","Jeep01"};
    static const char *const models[3]={"Driller01.v3m","APC.v3m","Jeep01.v3m"};
    static const char *const cockpits[3]={"driller01.vfx","APC.vfx","jeep.vfx"};
    const float origin[3]={0,0,0},identity[9]={1,0,0,0,1,0,0,0,1};float seat_pose[12];
    scene_vehicle_profile_pack *p=NULL;scene_apc_secondary_state secondary={0};uint32_t cap;int st;
    if(!out||*out||!chassis||!tables||!archive||!maps||!map_count||profile<1||profile>3||
       !isfinite(gravity)||gravity<0||budget<sizeof(*p))return RF_RANGE;
    if(strcmp(chassis->model,models[profile-1]))return RF_FORMAT;
    p=calloc(1,sizeof(*p));if(!p)return RF_IO;p->chassis=chassis;p->profile=profile;
    p->resident_bytes=p->peak_bytes=sizeof(*p);p->seat=p->gunner_seat=p->primary_muzzle=p->secondary_muzzle=-1;
    st=rf_static_model_tags_find(&chassis->tags,(rf_model_name){"interface_1",11},&p->seat);if(st)goto fail;
    st=rf_static_model_tag_place(&chassis->tags,p->seat,identity,origin,seat_pose);if(st)goto fail;
    if(profile==3){st=rf_static_model_tags_find(&chassis->tags,(rf_model_name){"interface_2",11},&p->gunner_seat);if(st)goto fail;
        st=rf_static_model_tag_place(&chassis->tags,p->gunner_seat,identity,origin,seat_pose);if(st)goto fail;}
    st=scene_vehicle_profile_physics_prepare(profile,chassis,origin,identity,gravity,&p->physics);if(st)goto fail;
    p->view.handle=-1;p->view.type=0;p->view.class_type=(int32_t)chassis->physics.authored.use_kind;
    p->view.linked_handle=-1;p->view.weapons[0]=p->view.weapons[1]=-1;p->view.base_speed=chassis->movement.speed;
    p->view.occupant_count=profile==3?2:1; /* Caller installs real occupant words on publication. */
    if(profile==1){
        cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,512*1024);
        st=scene_driller_bit_animation_open(&chassis->tags,archive,maps,map_count,cap,&p->bits);if(st)goto fail;
        scene_vehicle_profile_pack_account(p,p->bits->resident_bytes,p->bits->peak_bytes);
    }
    cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,2*1024*1024);
    st=scene_vehicle_cockpit_open(cockpits[profile-1],archive,maps,map_count,cap,&p->cockpit);if(st)goto fail;
    scene_vehicle_profile_pack_account(p,p->cockpit->resident_bytes,p->cockpit->peak_bytes);
    /* Table readers use a sequential scratch allowance; do not charge it as
     * permanent per-profile storage. Existing512KiB caps remain intact. */
    cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,512*1024);
    if(p->resident_bytes+cap>p->peak_bytes)p->peak_bytes=p->resident_bytes+cap;
    if(!cap){st=RF_RANGE;goto fail;}
    if(profile==1){st=scene_driller_weapon_open(tables,&chassis->tags,cap,&p->drill);if(st)goto fail;}
    if(profile==2){
        st=scene_vehicle_primary_apc_open(tables,&chassis->tags,cap,&p->primary,&p->primary_initial,&p->primary_muzzle);if(st)goto fail;
        st=scene_apc_secondary_open(tables,&chassis->tags,cap,&secondary);if(st)goto fail;
        p->secondary=secondary.definition;p->secondary_muzzle=secondary.muzzle;
        st=scene_vehicle_aim_limits_load(tables,classes[profile-1],cap,&p->aim_limits);if(st)goto fail;
        cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,512*1024);
        st=scene_apc_secondary_visual_open(archive,maps,map_count,cap,&p->mortar);if(st)goto fail;
        scene_vehicle_profile_pack_account(p,p->mortar->resident_bytes,p->mortar->peak_bytes);
    }
    if(profile==3){
        cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,576*1024);
        st=scene_jeep_gun_resources_open(archive,maps,map_count,&chassis->tags,cap,&p->gun);if(st)goto fail;
        scene_vehicle_profile_pack_account(p,p->gun->resident_bytes,p->gun->peak_bytes);
        cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,512*1024);
        if(p->resident_bytes+cap>p->peak_bytes)p->peak_bytes=p->resident_bytes+cap;
        if(!cap){st=RF_RANGE;goto fail;}
        st=scene_vehicle_primary_load(tables,"Jeep Gun",cap,&p->primary);if(st)goto fail;p->primary_muzzle=p->gun->muzzle;
        st=scene_vehicle_aim_limits_load(tables,"Jeep01",cap,&p->aim_limits);if(st)goto fail;
    }
    p->primary_reserve=(int32_t)p->primary.capacity;p->secondary_reserve=(int32_t)p->secondary.capacity;p->primary_random.value=1;
    cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,512*1024);
    if(p->resident_bytes+cap>p->peak_bytes)p->peak_bytes=p->resident_bytes+cap;
    if(!cap){st=RF_RANGE;goto fail;}
    st=scene_vehicle_damage_open(classes[profile-1],&p->damage,tables,chassis,0,0,0,cap);if(st)goto fail;
    if(p->resident_bytes>budget||p->peak_bytes>budget){st=RF_RANGE;goto fail;}
    *out=p;return RF_OK;
fail:scene_vehicle_profile_pack_close(&p);return st;
}
/* Merge all extras once before renderer submission. All admission/allocation
 * precedes image transfers, so rejection preserves both owners. Chassis images
 * are deliberately excluded: their existing class base/count must survive. */
static int scene_vehicle_profile_pack_materials_merge(scene_vehicle_profile_pack *p,rf_materials *scene,
    uint32_t texture_capacity,uint32_t material_byte_budget)
{
    rf_materials *banks[3]={0};uint32_t *bases[3]={0},*counts[3]={0};
    uint64_t extra_count=0,extra_bytes=0,pack_released=0;rf_material *combined;
    if(!p||!scene||!p->cockpit||!p->cockpit->materials||p->merged||p->cockpit->merged||
       texture_capacity>RF_CAMPAIGN_TEXTURE_SLOTS||scene->count>texture_capacity||scene->allocated_bytes>material_byte_budget)return RF_RANGE;
    if(p->bits){banks[0]=&p->bits->model.materials.textures;bases[0]=&p->bit_base;counts[0]=&p->bit_textures;}
    if(p->gun){banks[1]=&p->gun->model->materials.textures;bases[1]=&p->gun_base;counts[1]=&p->gun_textures;}
    if(p->mortar){banks[2]=&p->mortar->materials.textures;bases[2]=&p->mortar_base;counts[2]=&p->mortar_textures;}
    for(uint32_t i=0;i<3;i++)if(banks[i]){
        const rf_materials *b=banks[i];if(!b->count||!b->items||b->allocated_bytes<(uint64_t)b->count*sizeof(rf_material))return RF_FORMAT;
        extra_count+=b->count;extra_bytes+=b->allocated_bytes;pack_released+=b->allocated_bytes;
    }
    if(p->cockpit->materials->textures.texture_count>64)return RF_FORMAT;
    for(uint32_t i=0;i<p->cockpit->materials->textures.texture_count;i++){
        const rf_particle_animation *a=&p->cockpit->materials->textures.textures[i].animation;
        if(!a->count||!a->images)return RF_FORMAT;extra_count+=a->count;
        for(uint32_t j=0;j<a->count;j++){
            if(!a->images[j].rgba||!a->images[j].bytes)return RF_FORMAT;
            extra_bytes+=sizeof(rf_material)+(uint64_t)a->images[j].bytes;pack_released+=a->images[j].bytes;
        }
    }
    if(extra_count>texture_capacity-scene->count||extra_bytes>material_byte_budget-scene->allocated_bytes||
       extra_bytes>UINT32_MAX||pack_released>p->resident_bytes)return RF_RANGE;
    combined=realloc(scene->items,(scene->count+(uint32_t)extra_count)*sizeof(*combined));if(!combined)return RF_IO;
    scene->items=combined;
    for(uint32_t i=0;i<3;i++)if(banks[i]){
        rf_materials *b=banks[i];*bases[i]=scene->count;*counts[i]=b->count;
        memcpy(combined+scene->count,b->items,b->count*sizeof(*combined));scene->count+=b->count;
        scene->loaded+=b->loaded;scene->missing+=b->missing;free(b->items);memset(b,0,sizeof(*b));
    }
    for(uint32_t i=0;i<p->cockpit->materials->textures.texture_count;i++){
        rf_particle_animation *a=&p->cockpit->materials->textures.textures[i].animation;
        p->cockpit->texture_base[i]=scene->count;
        for(uint32_t j=0;j<a->count;j++){
            combined[scene->count++]=(rf_material){a->images[j],RF_OK,a->archive_index};
            ++scene->loaded;memset(a->images+j,0,sizeof(*a->images));++p->cockpit->texture_frames;
        }
    }
    scene->allocated_bytes+=(uint32_t)extra_bytes;p->material_bytes=(uint32_t)extra_bytes;
    p->resident_bytes-=(uint32_t)pack_released;p->cockpit->merged=1;p->merged=1;return RF_OK;
}
#endif

