#ifndef RF_SCENE_VEHICLE_ALLEGIANCE_INCLUDED
#define RF_SCENE_VEHICLE_ALLEGIANCE_INCLUDED
/* Original Set_Friendliness4bc280 ->489f70/4895f0 writes the complete word
 * at object+1f8. OFF4b9f80 does nothing. L1S3 events9619/9636 both write0
 * to APC26, initially1. No occupancy, hostility or route policy is inferred.
 * Include after selected runtime; passive owners have no skeletal view. */
static rf_damage_effect_state *scene_vehicle_allegiance_owner(uint32_t handle)
{
    scene_driller_damage_runtime *active=scene_driller_damage_owner;
    if(scene_driller_damage_runtime_valid(active)&&active->damage.state.effects.handle==handle&&
       rf_object_registry_lookup(&campaign_registry,handle)==&active->entry->registration)
        return &active->damage.state.effects;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        scene_passive_vehicle *p=campaign_passive_vehicles+i;
        if(p->handle==handle&&p->damage.state.effects.handle==handle&&
           rf_object_registry_lookup(&campaign_registry,handle)==p)return &p->damage.state.effects;
    }
    return NULL;
}
uint32_t rf_scene_vehicle_allegiance[8];
static int scene_vehicle_set_friendliness(uint32_t handle,uint32_t value)
{
    rf_damage_effect_state *owner=scene_vehicle_allegiance_owner(handle);
    if(!owner)return RF_NOT_FOUND;
    uint32_t selected=scene_driller_damage_owner&&owner==&scene_driller_damage_owner->damage.state.effects;
    ++rf_scene_vehicle_allegiance[0];++rf_scene_vehicle_allegiance[selected?1:2];
    rf_scene_vehicle_allegiance[3]=handle;rf_scene_vehicle_allegiance[4]=owner->affiliation;
    rf_scene_vehicle_allegiance[5]=value;rf_scene_vehicle_allegiance[6]=selected?(uint32_t)campaign_authored_vehicle_uid:0;
    if(!selected)for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)
        if(campaign_passive_vehicles[i].handle==handle)rf_scene_vehicle_allegiance[6]=campaign_passive_vehicles[i].uid;
    owner->affiliation=value;return RF_OK;
}
/* Every row binds one unique authored UID to its current selected/passive
 * owner. Secondary simulation borrows the passive damage owner. Parked RFSW
 * banks own no damage copy; ordinary switching transfers this whole owner. */
static int scene_vehicle_allegiance_live(uint32_t index,uint32_t *uid,
    rf_damage_effect_state **owner,uint32_t *authored)
{
    uint32_t found=0,handle;
    if(!uid||!owner||!authored)return RF_RANGE;
    if(campaign_authored_vehicle_uid){
        if(!index){
            *uid=(uint32_t)campaign_authored_vehicle_uid;handle=campaign_authored_vehicle_handle;
        }else {--index;if(index>=campaign_passive_vehicle_count)return RF_RANGE;
            *uid=campaign_passive_vehicles[index].uid;handle=campaign_passive_vehicles[index].handle;}
    }else {if(index>=campaign_passive_vehicle_count)return RF_RANGE;
        *uid=campaign_passive_vehicles[index].uid;handle=campaign_passive_vehicles[index].handle;}
    if(!*uid||*uid==UINT32_MAX||!(*owner=scene_vehicle_allegiance_owner(handle)))return RF_FORMAT;
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)
        if((uint32_t)campaign_seeds.records.items[i].record.uid==*uid){
            *authored=campaign_seeds.items[i].spawn.friendliness;++found;
        }
    return found==1?RF_OK:RF_FORMAT;
}
static uint32_t scene_vehicle_allegiance_count(void)
{return campaign_passive_vehicle_count+(campaign_authored_vehicle_uid!=0);}
static uint32_t scene_vehicle_allegiance_legacy_save_allowed(void)
{
    uint32_t count=scene_vehicle_allegiance_count(),uid,authored;rf_damage_effect_state *owner;
    for(uint32_t i=0;i<count;i++)
        if(scene_vehicle_allegiance_live(i,&uid,&owner,&authored)||owner->affiliation!=authored)return 0;
    return 1;
}
/* RFAL1 is optional, inside RFNS and outside RFPV/RFSV/RFVA. Header16:
 * magic,version,inner bytes,override count; each row8: authored UID,value.
 * The RFWC source identity/checksum binds level records and payload. Omit it
 * when every owner matches source, preserving all previous wire layouts.
 * Values remain full uint32, including0xffffffff; only UIDs are constrained. */
enum { SCENE_VEHICLE_ALLEGIANCE_HEADER=16,SCENE_VEHICLE_ALLEGIANCE_ROW=8 };
typedef struct scene_vehicle_allegiance_row {
    uint32_t uid,value;rf_damage_effect_state *owner;
} scene_vehicle_allegiance_row;
struct scene_vehicle_allegiance_stage {
    uint32_t count,bytes;scene_vehicle_allegiance_row *rows;
};
static uint32_t scene_vehicle_allegiance_bytes(const scene_vehicle_allegiance_stage *stage)
{return stage?stage->bytes:0;}
static void scene_vehicle_allegiance_close(scene_vehicle_allegiance_stage **stage)
{if(stage&&*stage){free((*stage)->rows);free(*stage);*stage=NULL;}}
static int scene_vehicle_allegiance_capture(unsigned char *p,uint32_t capacity,uint32_t inner,uint32_t *written)
{
    uint32_t count=scene_vehicle_allegiance_count(),changed=0,uid,authored;rf_damage_effect_state *owner;int status;
    uint64_t bytes;
    if(!p||!written||inner>capacity||count>RF_OBJECT_CAPACITY)return RF_RANGE;
    /* Validate everything before moving caller bytes. Live owners are stable
     * during this synchronous capture; the second pass cannot change identity. */
    for(uint32_t i=0;i<count;i++){
        status=scene_vehicle_allegiance_live(i,&uid,&owner,&authored);if(status)return status;
        for(uint32_t j=0;j<i;j++){
            uint32_t previous,unused;rf_damage_effect_state *other;
            status=scene_vehicle_allegiance_live(j,&previous,&other,&unused);if(status)return status;
            if(previous==uid||other==owner)return RF_FORMAT;
        }
        changed+=owner->affiliation!=authored;
    }
    if(!changed){*written=inner;return RF_OK;}
    bytes=SCENE_VEHICLE_ALLEGIANCE_HEADER+(uint64_t)inner+(uint64_t)changed*SCENE_VEHICLE_ALLEGIANCE_ROW;
    if(bytes>capacity||bytes>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    memmove(p+SCENE_VEHICLE_ALLEGIANCE_HEADER,p,inner);memcpy(p,"RFAL",4);
    scene_history_put(p+4,1);scene_history_put(p+8,inner);scene_history_put(p+12,changed);
    unsigned char *row=p+SCENE_VEHICLE_ALLEGIANCE_HEADER+inner;
    for(uint32_t i=0;i<count;i++){
        (void)scene_vehicle_allegiance_live(i,&uid,&owner,&authored);
        if(owner->affiliation==authored)continue;
        scene_history_put(row,uid);scene_history_put(row+4,owner->affiliation);row+=SCENE_VEHICLE_ALLEGIANCE_ROW;
    }
    *written=(uint32_t)bytes;return RF_OK;
}
static int scene_vehicle_allegiance_prepare(const unsigned char *p,uint32_t bytes,uint32_t budget,
    scene_vehicle_allegiance_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    uint32_t count=scene_vehicle_allegiance_count(),overrides,nested;uint64_t cost,required;
    scene_vehicle_allegiance_stage *stage=NULL;int status=RF_FORMAT;
    if(!out||*out||!inner||!inner_bytes||(!p&&bytes))return RF_RANGE;
    *inner=p;*inner_bytes=bytes;
    if(bytes<4||memcmp(p,"RFAL",4))return RF_OK; /* Exact legacy payload. */
    if(bytes<SCENE_VEHICLE_ALLEGIANCE_HEADER||scene_history_word(p+4)!=1)return RF_FORMAT;
    nested=scene_history_word(p+8);overrides=scene_history_word(p+12);
    required=SCENE_VEHICLE_ALLEGIANCE_HEADER+(uint64_t)nested+(uint64_t)overrides*SCENE_VEHICLE_ALLEGIANCE_ROW;
    cost=sizeof(*stage)+(uint64_t)count*sizeof(*stage->rows);
    if(!nested||!overrides||overrides>count||count>RF_OBJECT_CAPACITY||required!=bytes||
       required>RF_CHECKPOINT_FILE_MAX||cost>budget)return RF_FORMAT;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;
    stage->rows=calloc(count,sizeof(*stage->rows));if(!stage->rows){free(stage);return RF_IO;}
    stage->count=count;stage->bytes=(uint32_t)cost;
    for(uint32_t i=0;i<count;i++){
        scene_vehicle_allegiance_row *r=stage->rows+i;
        status=scene_vehicle_allegiance_live(i,&r->uid,&r->owner,&r->value);if(status)goto fail;
        for(uint32_t j=0;j<i;j++)if(stage->rows[j].uid==r->uid||stage->rows[j].owner==r->owner){status=RF_FORMAT;goto fail;}
    }
    for(uint32_t i=0;i<overrides;i++){
        const unsigned char *row=p+SCENE_VEHICLE_ALLEGIANCE_HEADER+nested+i*SCENE_VEHICLE_ALLEGIANCE_ROW;
        uint32_t uid=scene_history_word(row),slot;
        for(slot=0;slot<count&&stage->rows[slot].uid!=uid;slot++){}
        if(slot==count){status=RF_FORMAT;goto fail;}
        for(uint32_t j=0;j<i;j++)if(scene_history_word(p+SCENE_VEHICLE_ALLEGIANCE_HEADER+nested+j*SCENE_VEHICLE_ALLEGIANCE_ROW)==uid){status=RF_FORMAT;goto fail;}
        stage->rows[slot].value=scene_history_word(row+4);
    }
    *out=stage;*inner=p+SCENE_VEHICLE_ALLEGIANCE_HEADER;*inner_bytes=nested;return RF_OK;
fail:scene_vehicle_allegiance_close(&stage);return status;
}
/* After all fallible admission and selected/passive/secondary publication.
 * Addresses are fresh, generation-checked live owners, never wire pointers. */
static void scene_vehicle_allegiance_assign(const scene_vehicle_allegiance_stage *stage)
{
    if(!stage)return;
    for(uint32_t i=0;i<stage->count;i++)stage->rows[i].owner->affiliation=stage->rows[i].value;
}
#endif
