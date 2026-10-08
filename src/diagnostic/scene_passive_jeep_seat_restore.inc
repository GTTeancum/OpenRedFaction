#ifndef RF_SCENE_PASSIVE_JEEP_SEAT_RESTORE_INCLUDED
#define RF_SCENE_PASSIVE_JEEP_SEAT_RESTORE_INCLUDED
/* Include after scene_world_vehicle_restore.inc. Candidate-only clearance for
 * the stationary passive Jeep subset: RFNS, RFVA2 and RFSW were all admitted.
 * At most128 occupied seats and8 spheres per shape; no live pose substitution,
 * resource allocation, or publication. Other passive owners retain their
 * existing restore contract and are obstacles at their actual saved poses. */
uint32_t rf_scene_passive_jeep_restore[8]; /* calls,hosts,hull world,pairs,hostUID,actorUID,phase,status */
uint32_t rf_scene_passive_jeep_restore_audit[4]; /* runs,checks,rejections,error */
static int scene_passive_jeep_seat_restore_shape(const scene_stream *stream,
    const scene_passive_vehicle_checkpoint_stage *passive,uint32_t index,
    rf_checkpoint_placement *out,rf_physics_sphere spheres[8])
{
    const scene_passive_vehicle *owner;const scene_passive_vehicle_saved *saved;uint32_t kind,count;
    if(!stream||!passive||!out||!spheres||index>=passive->count||
       index>=campaign_passive_vehicle_count)return RF_RANGE;
    owner=campaign_passive_vehicles+index;saved=passive->rows+index;kind=owner->resource_kind;
    if(!saved->loaded||saved->uid!=owner->uid||kind>=6||
       rf_object_registry_lookup(&campaign_registry,owner->handle)!=owner)return RF_FORMAT;
    count=stream->passive_vehicle_sphere_count[kind];if(!count||count>8)return RF_FORMAT;
    memset(out,0,sizeof(*out));memset(spheres,0,8*sizeof(*spheres));
    for(uint32_t i=0;i<count;i++){
        memcpy(spheres[i].center,stream->passive_vehicle_spheres[kind][i].center,12);
        spheres[i].radius=stream->passive_vehicle_spheres[kind][i].radius;
    }
    out->spheres=spheres;out->count=count;out->query_flags=4;
    memcpy(out->position,saved->position,12);memcpy(out->basis,saved->basis,36);return RF_OK;
}
static int scene_passive_jeep_seat_restore_pair(const rf_checkpoint_placement *first,
    const rf_checkpoint_placement *second)
{
    ++rf_scene_passive_jeep_restore[3];
    return scene_vehicle_wreck_pair(first,second->spheres,second->count,second->position,second->basis);
}
static int scene_passive_jeep_seat_restore_admit(scene_stream *stream,const scene_world_restore_stage *world,
    const scene_world_player_stage *player,const scene_vehicle_checkpoint_record *vehicle,
    const scene_npc_seat_checkpoint_stage *seats,const scene_vehicle_switch_checkpoint_stage *parked,
    const scene_passive_vehicle_checkpoint_stage *passive,uint32_t active_hidden)
{
    uint32_t *trace=rf_scene_passive_jeep_restore;int status=RF_OK;
    if(!seats)return RF_OK;
    if(!stream||!world||!world->npcs||!player||!seats->admitted||
       seats->count>SCENE_NPC_SEAT_LIMIT)return RF_FORMAT;
    ++trace[0];trace[6]=trace[7]=0;
    for(uint32_t i=0;i<seats->count;i++){
        const scene_npc_seat_saved *seat=seats->rows+i;scene_npc_seat_host host;
        const float *position,*basis;uint32_t index,driver,retired,seed;
        rf_checkpoint_placement hull={0},actor={0},other={0};rf_physics_sphere spheres[8],other_spheres[8];
        scene_checkpoint_world_context context;rf_collision_room_location location;
        rf_entity_room_state room;rf_physics_support_contact support;
        if(!seat->active||!seat->passive_host)continue;
        trace[4]=seat->binding.host_uid;trace[5]=seat->actor_uid;trace[6]=1;
        if(!seat->pose_ready||!passive||passive->count!=campaign_passive_vehicle_count){status=RF_FORMAT;goto fail;}
        scene_npc_seat_binding identity=seat->binding;
        status=scene_npc_seat_host_read(&identity,&host);if(status)goto fail;
        status=scene_npc_seat_checkpoint_passive_host(seat,parked,passive,&host,&position,&basis);if(status)goto fail;
        for(index=0;index<passive->count;index++)if(passive->rows[index].uid==seat->binding.host_uid)break;
        if(index==passive->count){status=RF_FORMAT;goto fail;}
        status=scene_passive_jeep_seat_restore_shape(stream,passive,index,&hull,spheres);if(status)goto fail;
        for(driver=0;driver<world->npcs->count;driver++)if(world->npcs->entries[driver].saved.uid==seat->actor_uid)break;
        if(driver==world->npcs->count||world->npcs->entries[driver].handle!=seat->binding.actor||
           !scene_npc_seat_checkpoint_owns(seats,seat->binding.host_uid,seat->actor_uid)){status=RF_FORMAT;goto fail;}
        status=scene_npc_checkpoint_restore_placement(world->npcs,driver,&actor,&retired);if(status)goto fail;
        if(retired){status=RF_FORMAT;goto fail;}
        /* Seating grants no clearance exemption. Preserve only the existing
         * exact authored-hull contact policy used by active vehicle restore. */
        trace[6]=2;context=world->world;context.allow_no_contact=1;context.class_speed=0;context.dt=scene_step_seconds;
        status=scene_vehicle_switch_seed(seat->binding.host_uid,&seed);if(status)goto fail;
        context.authored_position=campaign_seeds.records.items[seed].record.position;
        context.authored_basis=campaign_seeds.records.items[seed].record.orientation[0];
        context.authored_props=world->authored_prop_views;
        context.authored_rotating_npc=context.authored_center_shift_npc=context.support_uid=0;
        context.npc_route_contact=context.player_route_contact=context.spawn_shell_uid=0;
        status=rf_geometry_collision_world_locate(context.world,hull.position,&location);if(status)goto fail;
        if(location.room==UINT32_MAX||location.room>=context.world->room_count){status=RF_NOT_FOUND;goto fail;}
        context.reference_room=location.room;
        status=scene_checkpoint_world_place(&context,&hull,&room,&support);if(status)goto fail;++trace[2];
        trace[6]=3;status=scene_passive_jeep_seat_restore_pair(&hull,&player->placement);if(status)goto fail;
        for(uint32_t j=0;j<world->npcs->count;j++){
            /* Only this exact admitted driver can intersect its own hull. */
            if(j==driver)continue;
            status=scene_npc_checkpoint_restore_placement(world->npcs,j,&other,&retired);if(status)goto fail;
            if(retired)continue;status=scene_passive_jeep_seat_restore_pair(&hull,&other);if(status)goto fail;
        }
        trace[6]=4;
        if(vehicle&&!active_hidden){
            scene_driller_physics physics;
            status=scene_vehicle_checkpoint_physics(stream,&vehicle->value.vehicle,&physics);if(status)goto fail;
            memset(&other,0,sizeof(other));other.spheres=physics.spheres;other.count=physics.sphere_count;
            memcpy(other.position,vehicle->value.vehicle.position,12);memcpy(other.basis,vehicle->value.vehicle.orientation,36);
            status=scene_passive_jeep_seat_restore_pair(&hull,&other);if(status)goto fail;
            status=scene_passive_jeep_seat_restore_pair(&actor,&other);if(status)goto fail;
        }
        trace[6]=5;
        for(uint32_t j=0;j<passive->count;j++){
            /* Visible wrecks retain hulls, including object dead bit2. */
            if(j==index||(passive->rows[j].object_flags&0x4000u))continue;
            status=scene_passive_jeep_seat_restore_shape(stream,passive,j,&other,other_spheres);if(status)goto fail;
            status=scene_passive_jeep_seat_restore_pair(&hull,&other);if(status)goto fail;
            status=scene_passive_jeep_seat_restore_pair(&actor,&other);if(status)goto fail;
        }
        ++trace[1];
    }
    trace[6]=6;return RF_OK;
fail:trace[7]=(uint32_t)status;return status;
}
/* Native bounded fixture only. The normal observer runs after late-load, so
 * read its exact opt-in config here before testing any candidate. No malformed
 * row ever reaches owner assignment; audit copies contain no resource owners. */
static int scene_passive_jeep_seat_restore_audit_enabled(uint32_t *enabled)
{
    static uint32_t checked,value;static int result;
    if(!enabled)return RF_RANGE;
    if(!checked){
        checked=1;
#ifdef RF_IMAGE_XBOX_NATIVE
        unsigned char config[16];FILE *file=fopen("D:\\campaign-passive-jeep-driver.bin","rb");
        if(file){
            size_t bytes=fread(config,1,sizeof(config),file);int extra=fgetc(file),closed=fclose(file);
            if(bytes!=sizeof(config)||extra!=EOF||closed||scene_history_word(config)!=UINT32_C(0x44504a50)||
               scene_history_word(config+4)<1||scene_history_word(config+4)>3||
               scene_history_word(config+8)||scene_history_word(config+12))result=RF_FORMAT;
            else value=scene_history_word(config+4)>=2;
        }
#endif
    }
    *enabled=value;return result;
}
static int scene_passive_jeep_seat_restore_audit(const scene_npc_seat_checkpoint_stage *seats,
    const scene_vehicle_switch_checkpoint_stage *parked,const scene_passive_vehicle_checkpoint_stage *passive)
{
    const scene_npc_seat_saved *saved=NULL;const scene_vehicle_parked_state *bank=NULL;
    const scene_passive_vehicle_saved *vehicle=NULL;scene_npc_seat_checkpoint_stage *candidate=NULL;
    scene_passive_vehicle *owner;campaign_npc_body *actor;uint32_t enabled,*probe=rf_scene_passive_jeep_restore_audit;
    int status=scene_passive_jeep_seat_restore_audit_enabled(&enabled);if(status||!enabled)return status;
    if(probe[0])return (int)probe[3];++probe[0];
    if(!seats||!seats->admitted||!parked||!parked->admitted||!passive){status=RF_FORMAT;goto done;}
    for(uint32_t i=0;i<seats->count;i++)if(seats->rows[i].active&&seats->rows[i].passive_host){saved=seats->rows+i;break;}
    if(!saved){status=RF_NOT_FOUND;goto done;}
    for(uint32_t i=0;i<parked->count;i++)if(parked->rows[i].uid==saved->binding.host_uid){bank=parked->rows+i;break;}
    for(uint32_t i=0;i<passive->count;i++)if(passive->rows[i].uid==saved->binding.host_uid){vehicle=passive->rows+i;break;}
    owner=scene_npc_seat_passive_jeep_host(&saved->binding);actor=scene_npc_seat_actor(&saved->binding);
    if(!bank||!vehicle||!owner||!actor){status=RF_FORMAT;goto done;}
    candidate=calloc(1,sizeof(*candidate)+sizeof(*candidate->rows));if(!candidate){status=RF_IO;goto done;}
    {
        scene_passive_vehicle owner_before=*owner;campaign_npc_body actor_before=*actor;
        scene_npc_seat_saved seat_before=*saved;scene_vehicle_parked_state bank_before=*bank;
        scene_passive_vehicle_saved vehicle_before=*vehicle;
        for(uint32_t test=0;test<22;test++){
            scene_vehicle_switch_checkpoint_stage switch_copy=*parked;scene_vehicle_parked_state parked_copy=*bank;
            scene_passive_vehicle_checkpoint_stage passive_copy=*passive;scene_passive_vehicle_saved vehicle_copy=*vehicle;
            memset(candidate,0,sizeof(*candidate)+sizeof(*candidate->rows));candidate->count=1;
            candidate->allocated_bytes=sizeof(*candidate)+sizeof(*candidate->rows);candidate->rows[0]=*saved;
            candidate->rows[0].pose_ready=candidate->rows[0].passive_host=0;
            switch_copy.count=1;switch_copy.rows=&parked_copy;passive_copy.count=1;passive_copy.rows=&vehicle_copy;
            switch(test){
            case 0:candidate->rows[0].player_gunner=1;break;
            case 1:candidate->rows[0].owns_host_flag=1;break;
            case 2:candidate->rows[0].binding.host_uid^=0x40000000u;break;
            case 3:candidate->rows[0].binding.host^=0x10000u;break;
            case 4:candidate->rows[0].binding.tag=-1;break;
            case 5:switch_copy.admitted=0;break;
            case 6:parked_copy.valid=0;break;
            case 7:parked_copy.profile=2;break;
            case 8:parked_copy.jeep_role=1;break;
            case 9:parked_copy.handle^=0x10000u;break;
            case 10:vehicle_copy.loaded=0;break;
            case 11:vehicle_copy.has_damage=0;break;
            case 12:vehicle_copy.attached=1;break;
            case 13:vehicle_copy.destroyed=1;break;
            case 14:vehicle_copy.health=0;break;
            case 15:vehicle_copy.object_flags|=0x4000u;break;
            case 16:vehicle_copy.position[0]+=.25f;break;
            case 17:vehicle_copy.basis[0]+=.25f;break;
            case 18:parked_copy.route.active=1;break;
            case 19:parked_copy.uid^=0x40000000u;break;
            case 20:parked_copy.seat=-1;break;
            case 21:passive_copy.version=1;break;
            }
            ++probe[1];status=scene_npc_seat_checkpoint_hosts(candidate,NULL,NULL,0,&switch_copy,&passive_copy);
            if(!status||candidate->admitted||candidate->rows[0].pose_ready||
               memcmp(owner,&owner_before,sizeof(*owner))||memcmp(actor,&actor_before,sizeof(*actor))||
               memcmp(saved,&seat_before,sizeof(*saved))||memcmp(bank,&bank_before,sizeof(*bank))||
               memcmp(vehicle,&vehicle_before,sizeof(*vehicle))){status=RF_FORMAT;goto done;}
            ++probe[2];
        }
    }
    status=RF_OK;
done:free(candidate);probe[3]=(uint32_t)status;return status;
}
#endif
