#ifndef RF_SCENE_VEHICLE_PROFILE_PACK_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_PACK_INCLUDED
/* Include after runtime/profile_physics_prepare and auxiliary resource helpers.
 * One ground/submarine extras owner. Chassis is BORROWED and never modified/closed;
 * world registries, scene profile and renderer are never published here.
 * Geometry must remain allocated until scene retained-model teardown. */

static void scene_vehicle_profile_pack_close(scene_vehicle_profile_pack **out)
{
    scene_vehicle_profile_pack *p;if(!out||!(p=*out))return;
    scene_driller_cockpit_close(&p->cockpit);scene_driller_bit_animation_close(&p->bits);
    scene_jeep_gun_resources_close(&p->gun);scene_driller_resources_close(&p->mortar);
    scene_driller_resources_close(&p->torpedo);
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
    static const char *const classes[4]={"Driller01","APC","Jeep01","sub"};
    static const char *const models[4]={"Driller01.v3m","APC.v3m","Jeep01.v3m","Sub_Mini01.v3m"};
    static const char *const cockpits[4]={"driller01.vfx","APC.vfx","jeep.vfx","sub.vfx"};
    const float origin[3]={0,0,0},identity[9]={1,0,0,0,1,0,0,0,1};float seat_pose[12];
    scene_vehicle_profile_pack *p=NULL;scene_apc_secondary_state secondary={0};uint32_t cap;int st;
    if(!out||*out||!chassis||!tables||!archive||!maps||!map_count||profile<1||profile>4||
       !isfinite(gravity)||gravity<0||budget<sizeof(*p))return RF_RANGE;
    if(strcmp(chassis->model,models[profile-1]))return RF_FORMAT;
    p=calloc(1,sizeof(*p));if(!p)return RF_IO;p->chassis=chassis;p->profile=profile;
    p->resident_bytes=p->peak_bytes=sizeof(*p);p->seat=p->gunner_seat=p->primary_muzzle=p->secondary_muzzle=-1;
    p->submarine_muzzle=-1;
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
    if(profile==4){
        scene_submarine_weapon_state weapon={0};
        st=scene_submarine_weapon_open(tables,&chassis->tags,"primary_1",cap,&weapon);if(st)goto fail;
        p->submarine_definition=weapon.definition;p->submarine_muzzle=weapon.muzzle;
        p->submarine_reserve=(int32_t)weapon.definition.capacity;
        p->submarine_parameters=(rf_vehicle_submersible_parameters){chassis->physics.authored.mass,
            chassis->movement.speed,chassis->movement.acceleration,chassis->rotation.maximum_velocity,
            chassis->rotation.acceleration,2};
        cap=scene_vehicle_profile_pack_cap(budget,p->resident_bytes,512*1024);
        st=scene_submarine_weapon_model_open(archive,maps,map_count,cap,&p->torpedo);if(st)goto fail;
        scene_vehicle_profile_pack_account(p,p->torpedo->resident_bytes,p->torpedo->peak_bytes);
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
    rf_materials *banks[4]={0};uint32_t *bases[4]={0},*counts[4]={0};
    uint64_t extra_count=0,extra_bytes=0,pack_released=0;rf_material *combined;
    if(!p||!scene||!p->cockpit||!p->cockpit->materials||p->merged||p->cockpit->merged||
       texture_capacity>RF_CAMPAIGN_TEXTURE_SLOTS||scene->count>texture_capacity||scene->allocated_bytes>material_byte_budget)return RF_RANGE;
    if(p->bits){banks[0]=&p->bits->model.materials.textures;bases[0]=&p->bit_base;counts[0]=&p->bit_textures;}
    if(p->gun){banks[1]=&p->gun->model->materials.textures;bases[1]=&p->gun_base;counts[1]=&p->gun_textures;}
    if(p->mortar){banks[2]=&p->mortar->materials.textures;bases[2]=&p->mortar_base;counts[2]=&p->mortar_textures;}
    if(p->torpedo){banks[3]=&p->torpedo->materials.textures;bases[3]=&p->torpedo_base;counts[3]=&p->torpedo_textures;}
    for(uint32_t i=0;i<4;i++)if(banks[i]){
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
    for(uint32_t i=0;i<4;i++)if(banks[i]){
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
/* Merged packs are safe for allocation-free gameplay/codec reads. */
static const scene_vehicle_profile_pack *scene_vehicle_profile_pack_get(const scene_stream *s,uint32_t profile)
{
    const scene_vehicle_profile_pack *p;
    if(!s||profile<1||profile>4)return NULL;p=s->vehicle_profile_packs[profile];
    return p&&p->profile==profile&&p->merged&&p->chassis&&p->cockpit&&p->cockpit->merged?p:NULL;
}
static int scene_vehicle_profile_pack_scene_images(const rf_materials *scene,uint32_t base,uint32_t count,uint64_t *bytes)
{
    if(!scene||!bytes||!count||!scene->items||base>scene->count||count>scene->count-base)return RF_FORMAT;
    for(uint32_t i=0;i<count;i++)*bytes+=scene->items[base+i].image.bytes;
    return RF_OK;
}
/* Adoption changes only close ownership. Existing stream fields remain exact
 * aliases, and every geometry/image address survives. On failure only the new
 * descriptor is freed; borrowed startup resources are never closed. */
static int scene_vehicle_profile_pack_adopt(scene_stream *s,uint32_t profile,const rf_materials *materials,
    float gravity,scene_vehicle_profile_pack **out)
{
    static const char *const model[4]={"Driller01.v3m","APC.v3m","Jeep01.v3m","Sub_Mini01.v3m"};
    const float origin[3]={0,0,0},identity[9]={1,0,0,0,1,0,0,0,1};
    scene_vehicle_profile_pack *p;uint64_t resident,released=0,material=0,pixels=0;int st;
    if(!s||!out||*out||profile<1||profile>4||!materials||!s->driller||!s->driller_cockpit||
       !s->driller_cockpit->merged||!s->driller_cockpit->materials||!isfinite(gravity)||gravity<0)return RF_RANGE;
    if(strcmp(s->driller->model,model[profile-1])||
       (profile==1?(!s->driller_bits||s->jeep_gun||s->apc_mortar||s->submarine_torpedo):
        profile==2?(!s->apc_mortar||s->driller_bits||s->jeep_gun||s->submarine_torpedo):
        profile==3?(!s->jeep_gun||s->driller_bits||s->apc_mortar||s->submarine_torpedo):
                   (!s->submarine_torpedo||s->driller_bits||s->jeep_gun||s->apc_mortar)))return RF_FORMAT;
    p=calloc(1,sizeof(*p));if(!p)return RF_IO;p->profile=profile;p->chassis=s->driller;
    p->cockpit=s->driller_cockpit;p->bits=s->driller_bits;p->gun=s->jeep_gun;p->mortar=s->apc_mortar;
    p->torpedo=s->submarine_torpedo;p->torpedo_base=s->submarine_torpedo_base;p->torpedo_textures=s->submarine_torpedo_textures;
    p->bit_base=s->driller_bit_base;p->bit_textures=s->driller_bit_textures;
    p->gun_base=s->jeep_gun_base;p->gun_textures=s->jeep_gun_textures;
    p->mortar_base=s->apc_mortar_base;p->mortar_textures=s->apc_mortar_textures;
    p->seat=p->gunner_seat=p->primary_muzzle=p->secondary_muzzle=-1;
    p->submarine_muzzle=-1;
    st=rf_static_model_tags_find(&p->chassis->tags,(rf_model_name){"interface_1",11},&p->seat);if(st)goto fail;
    if(profile==3){st=rf_static_model_tags_find(&p->chassis->tags,(rf_model_name){"interface_2",11},&p->gunner_seat);if(st)goto fail;}
    st=scene_vehicle_profile_physics_prepare(profile,p->chassis,origin,identity,gravity,&p->physics);if(st)goto fail;
    p->damage=s->driller_damage_prototype;
    p->view.handle=-1;p->view.type=0;p->view.class_type=(int32_t)p->chassis->physics.authored.use_kind;
    p->view.linked_handle=-1;p->view.weapons[0]=p->view.weapons[1]=-1;p->view.base_speed=p->chassis->movement.speed;
    p->view.occupant_count=profile==3?2:1;
    if(profile==1){p->drill=s->driller_weapon;
        p->drill.held_seconds=p->drill.damage_wait=p->drill.contact_seconds=p->drill.spin=p->drill.phase=0;
        p->drill.held=p->drill.contact_active=p->drill.accepted_cuts=p->drill.contacts=p->drill.damage_calls=0;
    }else if(profile<=3){p->primary=s->apc_primary.definition;p->primary_muzzle=s->apc_primary.muzzle;p->aim_limits=s->apc_aim_limits;}
    if(profile==2){p->secondary=s->apc_secondary.definition;p->secondary_muzzle=s->apc_secondary.muzzle;}
    if(profile==4){
        p->submarine_definition=s->submarine_weapon.definition;p->submarine_muzzle=s->submarine_weapon.muzzle;
        p->submarine_reserve=(int32_t)p->submarine_definition.capacity;
        p->submarine_parameters=(rf_vehicle_submersible_parameters){p->chassis->physics.authored.mass,
            p->chassis->movement.speed,p->chassis->movement.acceleration,p->chassis->rotation.maximum_velocity,
            p->chassis->rotation.acceleration,2};
    }
    p->primary_reserve=(int32_t)p->primary.capacity;p->secondary_reserve=(int32_t)p->secondary.capacity;p->primary_random.value=1;
    resident=sizeof(*p)+(uint64_t)p->cockpit->resident_bytes;
    if(p->bits){if(p->bits->model.materials.textures.items||p->bits->model.materials.textures.count){st=RF_FORMAT;goto fail;}
        st=scene_vehicle_profile_pack_scene_images(materials,p->bit_base,p->bit_textures,&pixels);if(st)goto fail;
        material+=(uint64_t)p->bit_textures*sizeof(rf_material);resident+=p->bits->resident_bytes;}
    if(p->gun){if(!p->gun->model||p->gun->model->materials.textures.items||p->gun->model->materials.textures.count){st=RF_FORMAT;goto fail;}
        st=scene_vehicle_profile_pack_scene_images(materials,p->gun_base,p->gun_textures,&pixels);if(st)goto fail;
        material+=(uint64_t)p->gun_textures*sizeof(rf_material);resident+=p->gun->resident_bytes;}
    if(p->mortar){if(p->mortar->materials.textures.items||p->mortar->materials.textures.count){st=RF_FORMAT;goto fail;}
        st=scene_vehicle_profile_pack_scene_images(materials,p->mortar_base,p->mortar_textures,&pixels);if(st)goto fail;
        material+=(uint64_t)p->mortar_textures*sizeof(rf_material);resident+=p->mortar->resident_bytes;}
    if(p->torpedo){if(p->torpedo->materials.textures.items||p->torpedo->materials.textures.count){st=RF_FORMAT;goto fail;}
        st=scene_vehicle_profile_pack_scene_images(materials,p->torpedo_base,p->torpedo_textures,&pixels);if(st)goto fail;
        material+=(uint64_t)p->torpedo_textures*sizeof(rf_material);resident+=p->torpedo->resident_bytes;}
    released=pixels+material;material+=pixels;
    if(p->cockpit->materials->textures.texture_count>64){st=RF_FORMAT;goto fail;}
    for(uint32_t i=0;i<p->cockpit->materials->textures.texture_count;i++){
        const rf_particle_animation *a=&p->cockpit->materials->textures.textures[i].animation;uint64_t image_bytes=0;
        if(!a->count)continue;if(!a->images){st=RF_FORMAT;goto fail;}
        for(uint32_t j=0;j<a->count;j++)if(a->images[j].rgba||a->images[j].bytes){st=RF_FORMAT;goto fail;}
        st=scene_vehicle_profile_pack_scene_images(materials,p->cockpit->texture_base[i],a->count,&image_bytes);if(st)goto fail;
        released+=image_bytes;material+=image_bytes+(uint64_t)a->count*sizeof(rf_material);
    }
    if(released>resident||resident-released>UINT32_MAX||material>UINT32_MAX){st=RF_RANGE;goto fail;}
    p->resident_bytes=(uint32_t)(resident-released);p->peak_bytes=p->resident_bytes;p->material_bytes=(uint32_t)material;p->merged=1;
    *out=p;return RF_OK;
fail:free(p);return st;
}
/* Resource ownership admission only. Caller separately admits owner identity,
 * gameplay dependencies, pending effects and placement before registry commit. */
static int scene_vehicle_profile_pack_exchange_admit(const scene_stream *s,uint32_t outgoing,const scene_vehicle_profile_pack *next)
{
    const scene_vehicle_profile_pack *old=scene_vehicle_profile_pack_get(s,outgoing);uint32_t old_kind,next_kind;
    if(!old||!next||next->profile<1||next->profile>4||next->profile==outgoing||
       scene_vehicle_profile_pack_get(s,next->profile)!=next)return RF_NOT_FOUND;
    old_kind=outgoing==4?0:outgoing+2;next_kind=next->profile==4?0:next->profile+2;
    if(s->driller!=old->chassis||s->passive_vehicle_resources[old_kind]||
       s->passive_vehicle_resources[next_kind]!=next->chassis||
       !s->driller_textures||!s->passive_vehicle_textures[next_kind]||
       s->driller_cockpit!=old->cockpit||s->driller_bits!=old->bits||s->jeep_gun!=old->gun||s->apc_mortar!=old->mortar||
       s->submarine_torpedo!=old->torpedo)return RF_NOT_FOUND;
    if(s->driller_bit_base!=old->bit_base||s->driller_bit_textures!=old->bit_textures||
       s->jeep_gun_base!=old->gun_base||s->jeep_gun_textures!=old->gun_textures||
       s->apc_mortar_base!=old->mortar_base||s->apc_mortar_textures!=old->mortar_textures||
       s->submarine_torpedo_base!=old->torpedo_base||s->submarine_torpedo_textures!=old->torpedo_textures)return RF_FORMAT;
    return RF_OK;
}
/* Infallible publication after exchange_admit and caller's full transaction
 * admission. Pack owns extras, stream aliases them; chassis close ownership
 * moves between the one active pointer and its passive-class NULL slot.
 * Mutable instance state is initialized here then overlaid from target bank.
 * No telemetry, global profile, registry, actor pose or audio is changed. */
static void scene_vehicle_profile_pack_publish(scene_stream *s,uint32_t outgoing,const scene_vehicle_profile_pack *next)
{
    uint32_t old_kind=outgoing==4?0:outgoing+2,next_kind=next->profile==4?0:next->profile+2;
    s->passive_vehicle_resources[old_kind]=s->driller;s->passive_vehicle_base[old_kind]=s->driller_base;
    s->passive_vehicle_textures[old_kind]=s->driller_textures;
    s->driller=s->passive_vehicle_resources[next_kind];s->driller_base=s->passive_vehicle_base[next_kind];
    s->driller_textures=s->passive_vehicle_textures[next_kind];s->passive_vehicle_resources[next_kind]=NULL;
    s->passive_vehicle_base[next_kind]=s->passive_vehicle_textures[next_kind]=0;
    s->driller->seat=next->seat;
    s->driller_cockpit=next->cockpit;s->driller_bits=next->bits;s->jeep_gun=next->gun;s->apc_mortar=next->mortar;
    s->submarine_torpedo=next->torpedo;s->submarine_torpedo_base=next->torpedo_base;s->submarine_torpedo_textures=next->torpedo_textures;
    s->driller_bit_base=next->bit_base;s->driller_bit_textures=next->bit_textures;
    s->jeep_gun_base=next->gun_base;s->jeep_gun_textures=next->gun_textures;
    s->apc_mortar_base=next->mortar_base;s->apc_mortar_textures=next->mortar_textures;
    s->driller_damage_prototype=next->damage;s->driller_weapon=next->drill;
    memset(&s->apc_primary,0,sizeof(s->apc_primary));s->apc_primary.definition=next->primary;
    s->apc_primary.scheduler=next->primary_initial;s->apc_primary.muzzle=next->primary_muzzle;
    s->apc_primary.reserve=next->primary_reserve;s->apc_primary.random=next->primary_random;
    memset(&s->apc_secondary,0,sizeof(s->apc_secondary));s->apc_secondary.definition=next->secondary;
    s->apc_secondary.muzzle=next->secondary_muzzle;s->apc_secondary.reserve=next->secondary_reserve;
    memset(&s->submarine_weapon,0,sizeof(s->submarine_weapon));
    s->submarine_weapon.definition=next->submarine_definition;
    s->submarine_weapon.muzzle=next->submarine_muzzle;s->submarine_weapon.reserve=next->submarine_reserve;
    s->apc_aim_limits=next->aim_limits;memset(&s->apc_aim,0,sizeof(s->apc_aim));s->apc_aim_active=0;
    memset(s->apc_aim_eye,0,sizeof(s->apc_aim_eye));memset(s->apc_aim_basis,0,sizeof(s->apc_aim_basis));
    memset(s->jeep_gun_pose,0,sizeof(s->jeep_gun_pose));memset(s->jeep_muzzle_pose,0,sizeof(s->jeep_muzzle_pose));
}
#endif
