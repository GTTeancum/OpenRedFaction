/* Early resource identity only; gameplay state still uses staged world load. */
static uint32_t scene_vehicle_switch_boot_order[RF_OBJECT_CAPACITY];
static uint32_t scene_vehicle_switch_boot_count,scene_vehicle_switch_boot_uid;
static uint32_t scene_vehicle_switch_boot_profiles,scene_vehicle_switch_original_profile;
static uint32_t scene_vehicle_switch_boot_profile_mask(void){return scene_vehicle_switch_boot_profiles;}
static uint32_t scene_vehicle_switch_boot_original_profile(void){return scene_vehicle_switch_original_profile;}
/* Saved inactive seat identities are hints for constructing the fresh bank.
 * Full RFNS/NPC semantic admission still precedes gameplay publication. */
static uint32_t scene_vehicle_switch_boot_seats[SCENE_NPC_SEAT_LIMIT][3];
static uint32_t scene_vehicle_switch_boot_seat_count;
static int scene_vehicle_switch_boot_retired_seat(uint32_t actor,uint32_t host,int32_t *tag)
{
    if(!tag||!scene_vehicle_switch_boot_uid)return 0;
    for(uint32_t i=0;i<scene_vehicle_switch_boot_seat_count;i++)
        if(scene_vehicle_switch_boot_seats[i][0]==actor&&scene_vehicle_switch_boot_seats[i][1]==host){
            *tag=(int32_t)scene_vehicle_switch_boot_seats[i][2];return 1;
        }
    return 0;
}
static int scene_vehicle_switch_boot_seats_read(const unsigned char *p,uint32_t bytes)
{
    /* RFSW has already passed its complete structural check. */
    uint32_t n=scene_history_word(p+8);p+=SCENE_VEHICLE_SWITCH_HEADER;(void)bytes;
    for(uint32_t depth=0;depth<6;depth++){
        if(n<4)return RF_OK;
        if(!memcmp(p,"RFNS",4)){
            if(n<16)return RF_FORMAT;
            uint32_t version=scene_history_word(p+4),inner=scene_history_word(p+8),count=scene_history_word(p+12);
            if((version!=1&&version!=2)||!count||count>SCENE_NPC_SEAT_LIMIT||16ull+inner+24ull*count!=n)return RF_FORMAT;
            const unsigned char *rows=p+16+inner;
            for(uint32_t i=0;i<count;i++){
                const unsigned char *r=rows+24*i;uint32_t actor=scene_history_word(r),host=scene_history_word(r+4);
                uint32_t tag=scene_history_word(r+8),kind=scene_history_word(r+12),active=scene_history_word(r+16),control=scene_history_word(r+20);
                if(!actor||actor>0x7fffffffu||!host||host>0x7fffffffu||tag>0x7fffffffu||active>1||control>1||
                   (kind!=SCENE_NPC_SEAT_JEEP&&kind!=SCENE_NPC_SEAT_TURRET)||(!active&&control))return RF_FORMAT;
                for(uint32_t j=0;j<i;j++)if(scene_history_word(rows+24*j)==actor)return RF_FORMAT;
                if(kind==SCENE_NPC_SEAT_JEEP&&!active){uint32_t *out=scene_vehicle_switch_boot_seats[scene_vehicle_switch_boot_seat_count++];out[0]=actor;out[1]=host;out[2]=tag;}
            }
            return RF_OK;
        }
        uint32_t row=0,count,inner,version;
        if(memcmp(p,"RFCL",4)&&memcmp(p,"RFTP",4)&&memcmp(p,"RFTU",4))return RF_OK;
        if(n<16)return RF_FORMAT;
        version=scene_history_word(p+4);inner=scene_history_word(p+8);count=scene_history_word(p+12);
        if(!memcmp(p,"RFCL",4)){if(version!=1)return RF_FORMAT;row=SCENE_CORPSE_LIFETIME_ROW;}
        else if(!memcmp(p,"RFTP",4)){if(version!=1||count!=SCENE_TURRET_PLAYER_CP_ROW)return RF_FORMAT;row=count;count=1;}
        else {if(version==1)row=SCENE_TURRET_CHECKPOINT_ROW;else if(version==2)row=SCENE_TURRET_GENERATED_CHECKPOINT_ROW;else if(version==3)row=SCENE_TURRET_RETIREMENT_ROW;else return RF_FORMAT;}
        if(16ull+inner+(uint64_t)count*row!=n)return RF_FORMAT;
        p+=16;n=inner;
    }
    return RF_FORMAT;
}
/* Read-only ordinary-save resource demand. This does not grant inventory or
 * publish gameplay; the normal world transaction still validates every owner. */
static int scene_vehicle_switch_boot_select(const rf_level *level,const char *tables_path)
{
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *path=scene_live_load_active?"redfaction-save":getenv("RF_REPLAY_WORLD_SNAPSHOT_IN");
#else
    rf_xbox_checkpoint_storage storage={0};uint32_t enabled=0,fixture=0;
#endif
    scene_world_snapshot_identity identity;rf_world_checkpoint world;
    rf_level_entity selected;rf_level_entity_spawn spawn;rf_level_entity_vitals vitals;
    uint32_t uid=0,profile=0,count=0,grouped=0,profiles=0;
    rf_checkpoint_file_selection selection={0};unsigned char *data=NULL;
    uint32_t bytes=0,i;int status=RF_OK;
    scene_vehicle_switch_boot_count=0;scene_vehicle_switch_boot_uid=0;scene_vehicle_switch_boot_seat_count=0;
    scene_vehicle_switch_boot_profiles=0;scene_vehicle_switch_original_profile=rf_scene_vehicle_enabled;
    if(!scene_live_load_active && scene_world_startup_snapshot_consumed)return RF_OK;
#ifdef RF_IMAGE_XBOX_NATIVE
    if(!scene_live_load_active){
        status=scene_world_storage_flag("D:\\world-fixture-load.flag",&fixture);if(status)return status;
        if(!fixture){status=scene_world_storage_flag("D:\\world-hdd-load.flag",&enabled);if(status||!enabled)return status;}
    }
#else
    if(!path||!*path)return RF_OK;
#endif
    status=scene_world_checkpoint_source_identity(level,tables_path,rf_scene_weapon_supply[3],identity.bytes);if(status)goto done;
    if(strlen(campaign_current_level)>=sizeof(identity.level)){status=RF_RANGE;goto done;}
    memset(identity.level,0,sizeof(identity.level));strcpy(identity.level,campaign_current_level);
    data=malloc(RF_CHECKPOINT_FILE_MAX);if(!data){status=RF_IO;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    if(fixture)status=rf_checkpoint_file_load(SCENE_WORLD_FIXTURE_BASE,data,RF_CHECKPOINT_FILE_MAX,&bytes,
        scene_world_snapshot_validate,&identity,&selection);
    else {status=scene_live_load_active&&scene_live_load_auto_source?
        rf_xbox_checkpoint_storage_open_world_auto(&storage,0):rf_xbox_checkpoint_storage_open_world(&storage,0);
        if(!status)status=rf_xbox_checkpoint_storage_load(&storage,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity);}
#else
    status=rf_checkpoint_file_load(path,data,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity,&selection);
#endif
    if(status)goto done;
    /* No wrapper is a normal legacy save, including an empty vehicle slice. */
    status=rf_world_checkpoint_decode(data,bytes,0,&world);if(status)goto done;
    const rf_world_checkpoint_slice *slice=world.sections+RF_WORLD_VEHICLE-1;
    status=scene_vehicle_switch_checkpoint_peek(slice->data,slice->bytes,&uid,&profile,&count);if(status||!uid)goto done;
    if(!campaign_authored_vehicle_uid||(scene_history_word(slice->data+4)==1&&profile!=rf_scene_vehicle_enabled)||profile<1||profile>3||
       uid>0x7fffffffu||!count||count>RF_OBJECT_CAPACITY){status=RF_FORMAT;goto done;}
    status=rf_level_entity_find(level,(int32_t)uid,&selected);if(status)goto done;
    if(scene_vehicle_selection_profile(selected.class_name)!=profile){status=RF_FORMAT;goto done;}
    {const rf_level_section *section=rf_level_find(level,0x30000);
     if(!section){status=RF_FORMAT;goto done;}
     status=scene_vehicle_selection_metadata(level,section,&selected,&spawn,&vitals);if(status)goto done;
     /* Runtime handoff cannot promote an authored hidden/parented/dead host.
      * The legacy boot host itself may be authored hidden (L1S3 APC9627),
      * become visible through scripts, then be selected again before saving. */
     if(spawn.seat_host_uid!=-1 || (vitals.health<=0&&vitals.health!=-1) ||
        ((spawn.creation_flags&2u)&&uid!=(uint32_t)campaign_authored_vehicle_uid)){
         status=RF_FORMAT;goto done;
     }}
    status=scene_vehicle_selection_grouped(level,uid,&grouped);if(status)goto done;
    if(grouped){status=RF_FORMAT;goto done;}
    profiles=1u<<profile;
    for(i=0;i<count;i++){
        uint32_t valid,row_profile;
        status=scene_vehicle_switch_checkpoint_row_identity(slice->data,slice->bytes,i,scene_vehicle_switch_boot_order+i,&valid,&row_profile);if(status)goto done;
        if(scene_vehicle_switch_boot_order[i]==uid){status=RF_FORMAT;goto done;}
        for(uint32_t j=0;j<i;j++)if(scene_vehicle_switch_boot_order[j]==scene_vehicle_switch_boot_order[i]){status=RF_FORMAT;goto done;}
        if(valid){
            rf_level_entity row;rf_level_entity_spawn row_spawn;rf_level_entity_vitals row_vitals;
            const rf_level_section *section=rf_level_find(level,0x30000);
            uint32_t row_uid=scene_vehicle_switch_boot_order[i],row_grouped=0;
            if(!section||row_uid>0x7fffffffu){status=RF_FORMAT;goto done;}
            status=rf_level_entity_find(level,(int32_t)row_uid,&row);if(status)goto done;
            if(scene_vehicle_selection_profile(row.class_name)!=row_profile){status=RF_FORMAT;goto done;}
            status=scene_vehicle_selection_metadata(level,section,&row,&row_spawn,&row_vitals);if(status)goto done;
            if(row_spawn.seat_host_uid!=-1||(row_vitals.health<=0&&row_vitals.health!=-1)||
               ((row_spawn.creation_flags&2u)&&row_uid!=(uint32_t)campaign_authored_vehicle_uid)){
                status=RF_FORMAT;goto done;
            }
            status=scene_vehicle_selection_grouped(level,row_uid,&row_grouped);if(status)goto done;
            if(row_grouped){status=RF_FORMAT;goto done;}profiles|=1u<<row_profile;
        }
    }
    status=scene_vehicle_switch_boot_seats_read(slice->data,slice->bytes);if(status)goto done;
    /* Publish the selected resource identity only after transport, source and
     * full saved order admission. No passive/active vehicle owner has been
     * registered yet; existing groups and movers remain unchanged. */
    scene_vehicle_switch_boot_count=count;scene_vehicle_switch_boot_uid=uid;
    scene_vehicle_switch_boot_profiles=profiles;rf_scene_vehicle_enabled=profile;
    campaign_authored_vehicle_uid=(int32_t)uid;
    memcpy(campaign_authored_vehicle_position,selected.position,12);
    memcpy(campaign_authored_vehicle_basis,selected.orientation,36);
    rf_scene_vehicle_selection[8]=uid;rf_scene_vehicle_selection[9]=profile;rf_scene_vehicle_selection[10]=3;
 done:
#ifdef RF_IMAGE_XBOX_NATIVE
    {int closed=scene_world_storage_close(&storage);if(!status)status=closed;}
#endif
    free(data);return status;
}
/* Restore the saved slot order before any checkpoint stage sees indexed
 * passive rows. Handles and group links stay with their authored objects. */
static int scene_vehicle_switch_boot_order_apply(void)
{
    if(!scene_vehicle_switch_boot_uid)return RF_OK;
    if(scene_vehicle_switch_boot_count!=campaign_passive_vehicle_count)return RF_FORMAT;
    /* Admit the whole UID set and owner registrations before the first swap.
     * A missing late row must not leave a partially reordered live array. */
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        uint32_t matches=0;
        const scene_passive_vehicle *owner=campaign_passive_vehicles+i;
        if(rf_object_registry_lookup(&campaign_registry,owner->handle)!=owner)return RF_NOT_FOUND;
        for(uint32_t j=0;j<campaign_passive_vehicle_count;j++)
            matches+=campaign_passive_vehicles[j].uid==scene_vehicle_switch_boot_order[i];
        if(matches!=1)return RF_FORMAT;
    }
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        uint32_t j;for(j=i;j<campaign_passive_vehicle_count;j++)
            if(campaign_passive_vehicles[j].uid==scene_vehicle_switch_boot_order[i])break;
        if(j==campaign_passive_vehicle_count)return RF_FORMAT;
        if(j==i)continue;
        scene_passive_vehicle *a=campaign_passive_vehicles+i,*b=campaign_passive_vehicles+j,temp;
        int status=rf_object_registry_exchange(&campaign_registry,a->handle,a,b,b->handle,b,a);if(status)return status;
        temp=*a;*a=*b;*b=temp;
    }
    return RF_OK;
}
