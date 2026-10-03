#ifndef RF_SCENE_VEHICLE_SELECTION_INCLUDED
#define RF_SCENE_VEHICLE_SELECTION_INCLUDED
/* Boot-time selection only: one existing runtime/resource profile. No owner
 * promotion, registration, table loading or mutations of campaign globals.
 * Include before rf_scene_set_campaign_spawn; stdlib/string/math and level
 * APIs must already be available. Preserve eight legacy host identities. */
typedef struct scene_vehicle_selection {
    uint32_t found,profile,legacy;
    rf_level_entity entity;
} scene_vehicle_selection;
/* scanned,supported,reject hidden/group/parent/dead/Fighter,eligible,
 * selected UID,profile,source(0 none/1 legacy/2 fallback),status */
uint32_t rf_scene_vehicle_selection[12];
static uint32_t scene_vehicle_selection_profile(const char *name)
{
    if(!strcmp(name,"Driller01"))return 1;
    if(!strcmp(name,"APC"))return 2;
    if(!strcmp(name,"Jeep01"))return 3;
    if(!strcmp(name,"sub"))return 4;
    return 0;
}
/* First-list UIDs are the actual general-object parent attachment inputs.
 * Stream every group to validate the section; never steal its child owner. */
static int scene_vehicle_selection_grouped(const rf_level *level,uint32_t uid,uint32_t *grouped)
{
    rf_level_group_reader reader;rf_level_group group;int status;
    *grouped=0;status=rf_level_groups_begin(level,&reader);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    while((status=rf_level_group_next(&reader,&group))==RF_OK){
        for(uint32_t i=0;i<group.ids_count[0];i++){
            uint32_t member;status=rf_level_group_id_at(level,&group,0,i,&member);if(status)return status;
            if(member==uid)*grouped=1;
        }
    }
    return status==RF_NOT_FOUND?RF_OK:status;
}
/* Raw metadata is needed only for a supported candidate. The existing full
 * record parsers validate every string/field and exact record exhaustion.
 * The separate64KiB scratch cap bounds modified records on stock64MiB. */
static int scene_vehicle_selection_metadata(const rf_level *level,const rf_level_section *section,
    const rf_level_entity *record,rf_level_entity_spawn *spawn,rf_level_entity_vitals *vitals)
{
    rf_level_owned_entity owned;unsigned char *raw;int status;
    if(!record->bytes||record->bytes>64u*1024u)return RF_RANGE;
    if(record->offset>section->size||record->bytes>section->size-record->offset)return RF_FORMAT;
    raw=malloc(record->bytes);if(!raw)return RF_IO;
    owned.record=*record;owned.raw=raw;
    status=rf_level_read(level,section,record->offset,raw,record->bytes);
    if(!status)status=rf_level_entity_spawn_read(&owned,spawn);
    if(!status)status=rf_level_entity_vitals_read(&owned,vitals);
    free(raw);return status;
}
static int scene_vehicle_select(const rf_level *level,scene_vehicle_selection *out)
{
    static const struct {const char *level,*name;int32_t uid;uint32_t profile;} legacy[]={
        {"L1S2.rfl","Driller01",8122,1},{"L1S3.rfl","APC",9627,2},
        {"L5S3.rfl","sub",3963,4},{"L5S4.rfl","sub",3955,4},
        {"L10S3.rfl","sub",6794,4},{"L12S1.rfl","Jeep01",7629,3},
        {"L13S3.rfl","Fighter01",8955,5},{"L18S2.rfl","Fighter01",10066,5}};
    scene_vehicle_selection value={0};rf_level_entity_reader reader;rf_level_entity record;
    double best=0;int status=RF_OK;
    memset(rf_scene_vehicle_selection,0,sizeof(rf_scene_vehicle_selection));
    if(!level||!out){status=RF_RANGE;goto done;}
    for(uint32_t j=0;j<3;j++)if(!isfinite(level->player_position[j])){status=RF_FORMAT;goto done;}
    for(uint32_t i=0;i<sizeof(legacy)/sizeof(legacy[0]);i++){
        if(strcmp(level->entry.name,legacy[i].level))continue;
        status=rf_level_entity_find(level,legacy[i].uid,&value.entity);if(status)goto done;
        if(strcmp(value.entity.class_name,legacy[i].name)){status=RF_FORMAT;goto done;}
        /* These identities include intentionally hidden campaign hosts. Keep
         * their exact existing selection/activation and old-save behavior. */
        value.found=value.legacy=1;value.profile=legacy[i].profile;
        rf_scene_vehicle_selection[0]=rf_scene_vehicle_selection[1]=rf_scene_vehicle_selection[7]=1;
        goto selected;
    }
    status=rf_level_entities_begin(level,&reader);
    if(status==RF_NOT_FOUND){status=RF_OK;goto selected;}if(status)goto done;
    while((status=rf_level_entity_next(&reader,&record))==RF_OK){
        rf_level_entity_spawn spawn;rf_level_entity_vitals vitals;uint32_t profile,grouped;double distance=0;
        ++rf_scene_vehicle_selection[0];profile=scene_vehicle_selection_profile(record.class_name);
        if(!profile){
            /* Fighter allegiance cannot establish manual-use eligibility:
             * player8955 and hostile L12S1 Fighters both have friendliness0.
             * masako_fighter also needs a different active model/resource. */
            if(!strcmp(record.class_name,"Fighter01")||!strcmp(record.class_name,"masako_fighter"))
                ++rf_scene_vehicle_selection[6];
            continue;
        }
        ++rf_scene_vehicle_selection[1];if(record.uid<=0){status=RF_FORMAT;goto done;}
        status=scene_vehicle_selection_metadata(level,&reader.section,&record,&spawn,&vitals);if(status)goto done;
        if(spawn.creation_flags&2u){++rf_scene_vehicle_selection[2];continue;}
        if(spawn.seat_host_uid!=-1){++rf_scene_vehicle_selection[4];continue;}
        if(vitals.health<=0&&vitals.health!=-1){++rf_scene_vehicle_selection[5];continue;}
        status=scene_vehicle_selection_grouped(level,(uint32_t)record.uid,&grouped);if(status)goto done;
        if(grouped){++rf_scene_vehicle_selection[3];continue;}
        for(uint32_t j=0;j<3;j++){double d=(double)record.position[j]-level->player_position[j];distance+=d*d;}
        if(!isfinite(distance)){status=RF_FORMAT;goto done;}
        ++rf_scene_vehicle_selection[7];
        if(!value.found||distance<best||(distance==best&&(uint32_t)record.uid<(uint32_t)value.entity.uid)){
            value.found=1;value.profile=profile;value.entity=record;best=distance;
        }
    }
    if(status!=RF_NOT_FOUND)goto done;status=RF_OK;
    if(value.found){
        /* Reuse the full UID scan's duplicate check. A duplicate selected UID
         * must not silently bind a different entity in the later seed pass. */
        status=rf_level_entity_find(level,value.entity.uid,&record);if(status)goto done;
    }
selected:
    if(value.found)for(uint32_t j=0;j<3;j++){
        if(!isfinite(value.entity.position[j])){status=RF_FORMAT;goto done;}
        for(uint32_t k=0;k<3;k++)if(!isfinite(value.entity.orientation[j][k])){status=RF_FORMAT;goto done;}
    }
    *out=value;
    if(value.found){rf_scene_vehicle_selection[8]=(uint32_t)value.entity.uid;
        rf_scene_vehicle_selection[9]=value.profile;rf_scene_vehicle_selection[10]=value.legacy?1u:2u;}
done:
    rf_scene_vehicle_selection[11]=(uint32_t)status;return status;
}
#endif
