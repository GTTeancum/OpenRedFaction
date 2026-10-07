#ifndef RF_SCENE_PASSIVE_VEHICLE_CHECKPOINT_INC
#define RF_SCENE_PASSIVE_VEHICLE_CHECKPOINT_INC
/* RFVA2 adds per-instance vitals/death to the pose rows. RFVA1 remains
 * readable, including older saves that listed only group-owned children.
 * The outer RFWC envelope supplies source identity and checksum; runtime
 * handles and group memberships are rebound from authored UIDs. */
enum { SCENE_PASSIVE_VEHICLE_HEADER=16,SCENE_PASSIVE_VEHICLE_ROW_V1=56,
       SCENE_PASSIVE_VEHICLE_ROW_V2=80 };
typedef struct scene_passive_vehicle_saved {
    uint32_t uid,attached;
    float position[3],basis[9];
    float health,armor;
    uint32_t object_flags,flags_814,flags_810,destroyed,has_damage,loaded;
} scene_passive_vehicle_saved;
typedef struct scene_passive_vehicle_checkpoint_stage {
    scene_passive_vehicle_saved *rows;
    uint32_t count,host_bytes,allocated_bytes,version;
} scene_passive_vehicle_checkpoint_stage;
static void scene_passive_vehicle_checkpoint_close(scene_passive_vehicle_checkpoint_stage **stage)
{if(stage&&*stage){free((*stage)->rows);free(*stage);*stage=NULL;}}
static int scene_passive_vehicle_checkpoint_capture(unsigned char *p,uint32_t capacity,
    uint32_t host_bytes,uint32_t *written)
{
    uint64_t required=SCENE_PASSIVE_VEHICLE_HEADER+(uint64_t)campaign_passive_vehicle_count*SCENE_PASSIVE_VEHICLE_ROW_V2+host_bytes;
    uint32_t i,j,group;unsigned char *row;
    if(!p||!written||host_bytes>capacity)return RF_RANGE;
    if(!campaign_passive_vehicle_count){*written=host_bytes;return RF_OK;}
    if(!campaign_passive_vehicles||!campaign_general_handles||required>capacity||
       required>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    for(i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *owner=campaign_passive_vehicles+i;uint32_t links=0;
        if(!owner->uid||owner->uid==UINT32_MAX||owner->attached>1||
           rf_object_registry_lookup(&campaign_registry,owner->handle)!=owner)return RF_FORMAT;
        for(j=0;j<i;j++)if(campaign_passive_vehicles[j].uid==owner->uid)return RF_FORMAT;
        for(group=0;group<campaign_group_runtime.count;group++){
            const rf_group_controller_view *view=campaign_controller_views+group;
            if(view->general_count&&!view->general_handles)return RF_FORMAT;
            for(j=0;j<view->general_count;j++)if(view->general_handles[j]==owner->handle)++links;
        }
        if((links>0)!=owner->attached)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(owner->pose.position[j]))return RF_FORMAT;
        for(j=0;j<9;j++)if(!isfinite(owner->pose.input_matrix[j]))return RF_FORMAT;
        if(owner->damage.state.effects.handle!=owner->handle||owner->damage.destroyed>1||
           owner->damage.destruction_pending||!isfinite(owner->damage.state.effects.health)||
           !isfinite(owner->damage.state.effects.armor)||
           fabsf(owner->damage.state.effects.health)>1e7f||
           owner->damage.state.effects.armor<0||owner->damage.state.effects.armor>1e7f||
           (owner->damage.destroyed&&owner->damage.state.effects.health>0))return RF_FORMAT;
    }
    memmove(p+SCENE_PASSIVE_VEHICLE_HEADER,p,host_bytes);
    memcpy(p,"RFVA",4);scene_history_put(p+4,2);scene_history_put(p+8,host_bytes);
    scene_history_put(p+12,campaign_passive_vehicle_count);
    row=p+SCENE_PASSIVE_VEHICLE_HEADER+host_bytes;
    for(i=0;i<campaign_passive_vehicle_count;i++,row+=SCENE_PASSIVE_VEHICLE_ROW_V2){
        const scene_passive_vehicle *owner=campaign_passive_vehicles+i;
        scene_history_put(row,owner->uid);scene_history_put(row+4,owner->attached);
        for(j=0;j<3;j++)scene_history_put_float(row+8+j*4,owner->pose.position[j]);
        for(j=0;j<9;j++)scene_history_put_float(row+20+j*4,owner->pose.input_matrix[j]);
        scene_history_put_float(row+56,owner->damage.state.effects.health);
        scene_history_put_float(row+60,owner->damage.state.effects.armor);
        scene_history_put(row+64,owner->damage.object_flags);
        scene_history_put(row+68,owner->damage.state.effects.flags_814);
        scene_history_put(row+72,owner->damage.state.effects.flags_810);
        scene_history_put(row+76,owner->damage.destroyed);
    }
    *written=(uint32_t)required;return RF_OK;
}
static int scene_passive_vehicle_checkpoint_prepare(const unsigned char *p,uint32_t bytes,
    uint32_t budget,scene_passive_vehicle_checkpoint_stage **out)
{
    scene_passive_vehicle_checkpoint_stage *stage;uint32_t count,host_bytes,version,row_bytes,i,j,slot;
    const unsigned char *row;uint64_t required;
    if(!out||*out||!p||bytes<SCENE_PASSIVE_VEHICLE_HEADER||memcmp(p,"RFVA",4))return RF_FORMAT;
    version=scene_history_word(p+4);host_bytes=scene_history_word(p+8);count=scene_history_word(p+12);
    if(version!=1&&version!=2)return RF_FORMAT;
    row_bytes=version==1?SCENE_PASSIVE_VEHICLE_ROW_V1:SCENE_PASSIVE_VEHICLE_ROW_V2;
    required=SCENE_PASSIVE_VEHICLE_HEADER+(uint64_t)host_bytes+(uint64_t)count*row_bytes;
    if(!count||count>campaign_passive_vehicle_count||
       (version==2&&count!=campaign_passive_vehicle_count)||required!=bytes||
       required>RF_CHECKPOINT_FILE_MAX||
       (uint64_t)sizeof(*stage)+(uint64_t)campaign_passive_vehicle_count*sizeof(*stage->rows)>budget)return RF_FORMAT;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;
    stage->rows=calloc(campaign_passive_vehicle_count,sizeof(*stage->rows));
    if(!stage->rows){free(stage);return RF_IO;}
    stage->count=campaign_passive_vehicle_count;stage->host_bytes=host_bytes;stage->version=version;
    stage->allocated_bytes=sizeof(*stage)+stage->count*sizeof(*stage->rows);
    /* RFVA1 may predate ordinary vehicle registration. Unlisted owners keep
     * their authored pose, group membership and initial vitals. */
    for(i=0;i<stage->count;i++){
        const scene_passive_vehicle *owner=campaign_passive_vehicles+i;
        scene_passive_vehicle_saved *saved=stage->rows+i;
        saved->uid=owner->uid;saved->attached=owner->attached;
        memcpy(saved->position,owner->pose.position,12);
        memcpy(saved->basis,owner->pose.input_matrix,36);
    }
    row=p+SCENE_PASSIVE_VEHICLE_HEADER+host_bytes;
    for(i=0;i<count;i++,row+=row_bytes){
        uint32_t uid=scene_history_word(row);scene_passive_vehicle_saved *saved;
        const scene_passive_vehicle *owner;
        if(version==2)slot=i;
        else for(slot=0;slot<stage->count && stage->rows[slot].uid!=uid;slot++);
        if(slot==stage->count){scene_passive_vehicle_checkpoint_close(&stage);return RF_FORMAT;}
        saved=stage->rows+slot;owner=campaign_passive_vehicles+slot;
        if(saved->loaded||uid!=owner->uid){scene_passive_vehicle_checkpoint_close(&stage);return RF_FORMAT;}
        saved->loaded=1;saved->attached=scene_history_word(row+4);
        if(saved->uid!=owner->uid||saved->attached>1||(saved->attached&&!owner->group_owned)||
           rf_object_registry_lookup(&campaign_registry,owner->handle)!=owner){
            scene_passive_vehicle_checkpoint_close(&stage);return RF_FORMAT;
        }
        for(j=0;j<3;j++){saved->position[j]=scene_history_float(row+8+j*4);
            if(!isfinite(saved->position[j])){scene_passive_vehicle_checkpoint_close(&stage);return RF_FORMAT;}}
        for(j=0;j<9;j++){saved->basis[j]=scene_history_float(row+20+j*4);
            if(!isfinite(saved->basis[j])){scene_passive_vehicle_checkpoint_close(&stage);return RF_FORMAT;}}
        if(version==2){
            saved->health=scene_history_float(row+56);saved->armor=scene_history_float(row+60);
            saved->object_flags=scene_history_word(row+64);saved->flags_814=scene_history_word(row+68);
            saved->flags_810=scene_history_word(row+72);saved->destroyed=scene_history_word(row+76);
            if(!isfinite(saved->health)||!isfinite(saved->armor)||fabsf(saved->health)>1e7f||
               saved->armor<0||saved->armor>1e7f||saved->destroyed>1||
               (saved->destroyed&&saved->health>0)){
                scene_passive_vehicle_checkpoint_close(&stage);return RF_FORMAT;
            }
            saved->has_damage=1;
        }
    }
    *out=stage;return RF_OK;
}
static int scene_passive_vehicle_checkpoint_validate(const scene_passive_vehicle_checkpoint_stage *stage)
{
    uint32_t group,i,capacity=0;
    if(!stage)return RF_OK;
    if(stage->count!=campaign_passive_vehicle_count||!stage->rows||!campaign_general_handles||
       !campaign_controller_views)return RF_FORMAT;
    for(group=0;group<campaign_group_runtime.count;group++){
        const rf_group_runtime_entry *entry=campaign_group_runtime.items+group;
        const rf_group_controller_view *view=campaign_controller_views+group;
        if(entry->kind==RF_GROUP_RUNTIME_EMPTY)continue;
        if(!entry->source||(entry->source->record.ids_count[0]&&!entry->source->ids[0]))return RF_FORMAT;
        capacity+=entry->source->record.ids_count[0];
        if(view->general_count&&!view->general_handles)return RF_FORMAT;
    }
    {uint32_t grouped=0;
     for(i=0;i<stage->count;i++)grouped+=campaign_passive_vehicles[i].group_owned;
     if(capacity<grouped)return RF_FORMAT;}
    for(i=0;i<stage->count;i++){
        rf_group_attached_pose candidate=campaign_passive_vehicles[i].pose;
        if(stage->rows[i].uid!=campaign_passive_vehicles[i].uid||
           rf_object_registry_lookup(&campaign_registry,campaign_passive_vehicles[i].handle)!=campaign_passive_vehicles+i||
           rf_group_pose_set_position(&candidate,stage->rows[i].position))return RF_FORMAT;
    }
    return RF_OK;
}
static void scene_passive_vehicle_checkpoint_assign(const scene_passive_vehicle_checkpoint_stage *stage)
{
    uint32_t group,i,used=0,detached=0;
    if(!stage)return;
    for(i=0;i<stage->count;i++){
        scene_passive_vehicle *owner=campaign_passive_vehicles+i;
        const scene_passive_vehicle_saved *saved=stage->rows+i;
        owner->attached=saved->attached;
        rf_group_pose_set_position(&owner->pose,saved->position);
        memcpy(owner->pose.pending,saved->position,12);
        memcpy(owner->pose.input_matrix,saved->basis,36);
        memcpy(owner->pose.output_matrix,saved->basis,36);
        memcpy(owner->pose.pending_matrix,saved->basis,36);
        if(saved->has_damage){
            owner->damage.state.effects.health=saved->health;
            owner->damage.state.effects.armor=saved->armor;
            owner->damage.state.effects.flags_814=saved->flags_814;
            owner->damage.state.effects.flags_810=saved->flags_810;
            owner->damage.state.effects.burn=0;
            owner->damage.state.effects.voice=UINT32_MAX;
            owner->damage.state.last_damage_time=0;
            owner->damage.state.responsible_handle=UINT32_MAX;
            owner->damage.state.burn_source=UINT32_MAX;
            owner->damage.object_flags=saved->object_flags;
            owner->damage.destroyed=saved->destroyed;
            owner->damage.destruction_pending=0;
        }
        memset(owner->velocity,0,12);owner->motion_valid=0;detached+=owner->group_owned&&!saved->attached;
    }
    for(group=0;group<campaign_group_runtime.count;group++){
        const rf_group_runtime_entry *entry=campaign_group_runtime.items+group;
        rf_group_controller_view *view=campaign_controller_views+group;
        uint32_t ref,count=0,capacity;
        if(entry->kind==RF_GROUP_RUNTIME_EMPTY)continue;
        capacity=entry->source->record.ids_count[0];
        for(ref=0;ref<capacity;ref++){
            uint32_t uid=entry->source->ids[0][ref];
            for(i=0;i<stage->count;i++)if(stage->rows[i].uid==uid&&stage->rows[i].attached){
                campaign_general_handles[used+count++]=campaign_passive_vehicles[i].handle;break;
            }
            /* Control membership was fully checked before the transaction.
             * Restore authored order without a fallible registry callback. */
            if(i==stage->count)for(i=0;i<campaign_group_control_child_count;i++)
                if(campaign_group_control_children[i].uid==uid){
                    campaign_general_handles[used+count++]=campaign_group_control_children[i].handle;break;
                }
        }
        view->general_count=count;
        view->general_handles=count?campaign_general_handles+used:NULL;
        used+=capacity;
    }
    rf_scene_passive_attachment[3]=detached;
}
#endif
