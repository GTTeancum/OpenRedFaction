#ifndef SCENE_BURNING_CHECKPOINT_INC
#define SCENE_BURNING_CHECKPOINT_INC
/* RFAP4 kind10: NPC burn owner, slot and damage cadence. RFNC stores target
 * vitals; these rows store the effect which continues changing those vitals.
 * Current gameplay deliberately excludes player ignition (entity.c retained
 * damage-effects admission); no unsupported player burn is invented here. */
typedef struct scene_burning_checkpoint_stage {
    scene_burning_record records[SCENE_BURNING_CAPACITY];
    uint32_t target_uid[SCENE_BURNING_CAPACITY],source_kind[SCENE_BURNING_CAPACITY],source_uid[SCENE_BURNING_CAPACITY];
    int32_t pain[SCENE_BURNING_CAPACITY][4];
    uint64_t seen;uint32_t count,pain_rng;
} scene_burning_checkpoint_stage;
/* restored count, first target UID, source kind/UID, lifetime, phase, health */
uint32_t rf_scene_burning_restored[7];
static int scene_burning_checkpoint_npc_uid(uint32_t handle,uint32_t *uid)
{
    uint32_t i;
    for(i=0;i<campaign_npc_body_count&&i<campaign_seeds.records.count;i++)
        if(campaign_npc_bodies[i].registration.handle==handle &&
           campaign_npc_bodies[i].persistence_registered){
            int32_t value=campaign_seeds.records.items[i].record.uid;
            if(value<=0)return RF_FORMAT;*uid=(uint32_t)value;return RF_OK;
        }
    return RF_NOT_FOUND;
}
static int scene_burning_checkpoint_npc_handle(uint32_t uid,uint32_t *handle)
{
    uint32_t i;
    for(i=0;i<campaign_npc_body_count&&i<campaign_seeds.records.count;i++)
        if((uint32_t)campaign_seeds.records.items[i].record.uid==uid &&
           campaign_npc_bodies[i].persistence_registered){
            *handle=campaign_npc_bodies[i].registration.handle;
            return *handle==UINT32_MAX?RF_FORMAT:RF_OK;
        }
    return RF_NOT_FOUND;
}
/* Source kinds:0 absent,1 local player,2 authored NPC,3 occupied/boardable
 * vehicle owner,4 passive vehicle owner. Registered-dead NPCs keep authored
 * attribution; removed-after-ignition sources currently reject capture because
 * unregister clears their handles. Never fabricate a process-local handle. */
static int scene_burning_checkpoint_source(uint32_t handle,uint32_t *kind,uint32_t *uid)
{
    uint32_t i;*kind=*uid=0;
    if(handle==UINT32_MAX)return RF_OK;
    if(handle==campaign_player_object.handle){*kind=1;return RF_OK;}
    if(!scene_burning_checkpoint_npc_uid(handle,uid)){*kind=2;return RF_OK;}
    if(campaign_authored_vehicle_uid>0&&handle==campaign_authored_vehicle_handle){
        *kind=3;*uid=(uint32_t)campaign_authored_vehicle_uid;return RF_OK;}
    for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==handle){
        *kind=4;*uid=campaign_passive_vehicles[i].uid;return *uid?RF_OK:RF_FORMAT;}
    return RF_NOT_FOUND;
}
static int scene_burning_checkpoint_source_handle(uint32_t kind,uint32_t uid,uint32_t *handle)
{
    uint32_t i;
    if(kind<2&&uid)return RF_FORMAT;
    if(!kind){*handle=UINT32_MAX;return RF_OK;}
    if(kind==1){*handle=campaign_player_object.handle;return RF_OK;}
    if(!uid||uid==UINT32_MAX)return RF_FORMAT;
    if(kind==2)return scene_burning_checkpoint_npc_handle(uid,handle);
    if(kind==3){
        if(uid!=(uint32_t)campaign_authored_vehicle_uid||!campaign_authored_vehicle_handle)return RF_FORMAT;
        *handle=campaign_authored_vehicle_handle;return RF_OK;
    }
    if(kind==4)for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].uid==uid){
        *handle=campaign_passive_vehicles[i].handle;return RF_OK;}
    return RF_FORMAT;
}
static int scene_burning_checkpoint_pain(const campaign_npc_body *owner,int32_t values[4])
{
    int32_t deadlines[3]={owner->pain.ai_timer,owner->pain.animation_lock,owner->pain.cooldown};
    int32_t now=(int32_t)(((uint64_t)combat_frame*1000/60)%RF_TIMER_PERIOD);uint32_t i;
    if(owner->pain.selected_action< -1||owner->pain.selected_action>=45)return RF_FORMAT;
    for(i=0;i<3;i++){
        int status;if(deadlines[i]<0){values[i]=-1;continue;}
        status=rf_timer_remaining(deadlines[i],now,values+i);if(status)return status;
        if(values[i]<0)values[i]=0;
    }
    values[3]=owner->pain.selected_action;return RF_OK;
}
static int scene_burning_checkpoint_owner_admit(uint32_t index)
{
    const campaign_npc_body *owner;const scene_burning_record *r;uint32_t slot,kind,uid;int32_t pain[4];int status;
    if(index>=campaign_npc_body_count)return RF_RANGE;
    owner=campaign_npc_bodies+index;slot=owner->damage.effects.burn;
    if(!slot||slot>SCENE_BURNING_CAPACITY)return RF_FORMAT;r=scene_burning_records+slot-1;
    if(!r->left||r->left>300||r->phase>=15||r->target!=owner->registration.handle||
       owner->damage.burn_source!=r->source||owner->damage.effects.health<=0||
       !isfinite(owner->damage.effects.class_health)||owner->damage.effects.class_health<=0||
       (owner->object_flags&(2u|0x4000u))||(owner->view.flags_810&1u))return RF_FORMAT;
    status=scene_burning_checkpoint_pain(owner,pain);if(status)return status;
    return scene_burning_checkpoint_source(r->source,&kind,&uid);
}
static uint32_t scene_burning_checkpoint_count(void)
{uint32_t i,n=0;for(i=0;i<SCENE_BURNING_CAPACITY;i++)n+=scene_burning_records[i].left!=0;return n;}
static int scene_burning_checkpoint_capture(unsigned char *output,uint32_t *rows)
{
    uint32_t i,n=0,index,target,kind,source,k;int32_t pain[4];int status;
    for(i=0;i<SCENE_BURNING_CAPACITY;i++){
        const scene_burning_record *burn=scene_burning_records+i;unsigned char *r;
        if(!burn->left)continue;
        if(!scene_burning_owner(burn->target,&index))return RF_FORMAT;
        status=scene_burning_checkpoint_owner_admit(index);if(status)return status;
        status=scene_burning_checkpoint_npc_uid(burn->target,&target);if(status)return status;
        status=scene_burning_checkpoint_source(burn->source,&kind,&source);if(status)return status;
        r=output+112*n++;checkpoint_put(r,10);checkpoint_put(r+4,i);checkpoint_put(r+8,target);
        checkpoint_put(r+12,kind);checkpoint_put(r+16,source);checkpoint_put(r+20,burn->left);checkpoint_put(r+24,burn->phase);
        status=scene_burning_checkpoint_pain(campaign_npc_bodies+index,pain);if(status)return status;
        for(k=0;k<4;k++)checkpoint_put(r+28+k*4,(uint32_t)pain[k]);
        checkpoint_put(r+44,combat_pain_random.value);
    }
    *rows=n;return RF_OK;
}
static int scene_burning_checkpoint_read(scene_burning_checkpoint_stage *stage,const unsigned char *r)
{
    uint32_t slot=checkpoint_u32(r+4),target=checkpoint_u32(r+8),kind=checkpoint_u32(r+12),source=checkpoint_u32(r+16),i;
    scene_burning_record *burn;
    if(slot>=SCENE_BURNING_CAPACITY||(stage->seen&(1ull<<slot))||!target||target==UINT32_MAX||
       !scene_flame_resources)return RF_FORMAT;
    for(i=48;i<112;i++)if(r[i])return RF_FORMAT;
    for(i=0;i<4;i++)stage->pain[slot][i]=(int32_t)checkpoint_u32(r+28+i*4);
    for(i=0;i<3;i++)if(stage->pain[slot][i]< -1||stage->pain[slot][i]>RF_TIMER_PERIOD/2)return RF_FORMAT;
    if(stage->pain[slot][3]< -1||stage->pain[slot][3]>=45||
       (stage->count&&stage->pain_rng!=checkpoint_u32(r+44)))return RF_FORMAT;
    stage->pain_rng=checkpoint_u32(r+44);
    for(i=0;i<SCENE_BURNING_CAPACITY;i++)if(stage->records[i].left&&stage->target_uid[i]==target)return RF_FORMAT;
    burn=stage->records+slot;burn->left=checkpoint_u32(r+20);burn->phase=checkpoint_u32(r+24);
    if(!burn->left||burn->left>300||burn->phase>=15||
       scene_burning_checkpoint_npc_handle(target,&burn->target)||
       scene_burning_checkpoint_source_handle(kind,source,&burn->source))return RF_FORMAT;
    stage->target_uid[slot]=target;stage->source_kind[slot]=kind;stage->source_uid[slot]=source;
    stage->seen|=1ull<<slot;++stage->count;return RF_OK;
}
static int scene_burning_checkpoint_admit(const scene_burning_checkpoint_stage *burning,
    const scene_npc_checkpoint_restore_stage *npcs)
{
    uint32_t i,j;
    if(!burning||!burning->count)return RF_OK;
    if(!npcs)return RF_FORMAT;
    for(i=0;i<SCENE_BURNING_CAPACITY;i++)if(burning->records[i].left){
        const scene_npc_checkpoint_restore_entry *entry=NULL;
        for(j=0;j<npcs->count;j++)if(npcs->entries[j].saved.uid==burning->target_uid[i]){entry=npcs->entries+j;break;}
        if(!entry||!entry->registered||entry->saved.retired||entry->saved.dead_pose||entry->saved.health<=0||
           (entry->flags&(2u|0x4000u)))return RF_FORMAT;
        /* RFNC carries the exact clip playback; only its saved pain action
         * supplies ownership for this additional non-looping action. */
        if(burning->pain[i][3]>=0){
            int32_t action=burning->pain[i][3],motion=entry->selection.mapping.actions[action];
            if(motion<0)return RF_FORMAT;
        }
        if(burning->source_kind[i]==2){
            for(j=0;j<npcs->count;j++)if(npcs->entries[j].saved.uid==burning->source_uid[i])break;
            if(j==npcs->count)return RF_FORMAT;
        }
    }
    return RF_OK;
}
static void scene_burning_checkpoint_assign(const scene_burning_checkpoint_stage *stage)
{
    uint32_t i,k;int32_t now=(int32_t)(((uint64_t)combat_frame*1000/60)%RF_TIMER_PERIOD);scene_burning_reset();
    memset(scene_burning_visual_links,0,sizeof(scene_burning_visual_links));
    memset(rf_scene_burning_restored,0,sizeof(rf_scene_burning_restored));
    if(!stage)return;
    memcpy(scene_burning_records,stage->records,sizeof(stage->records));
    rf_scene_burning_restored[0]=stage->count;
    if(stage->count)combat_pain_random.value=stage->pain_rng;
    for(i=0;i<SCENE_BURNING_CAPACITY;i++)if(stage->records[i].left){
        campaign_npc_body *owner=scene_burning_owner(stage->records[i].target,NULL);
        /* World preflight established each live target before this transaction. */
        if(!owner)continue;
        owner->damage.effects.burn=i+1;owner->damage.burn_source=stage->records[i].source;
        {int32_t restored[3];
         for(k=0;k<3;k++){
            int32_t remaining=stage->pain[i][k];
            restored[k]=remaining<0?-1:(int32_t)(((uint64_t)now+(uint32_t)remaining)>RF_TIMER_PERIOD?
                (uint64_t)now+(uint32_t)remaining-RF_TIMER_PERIOD:(uint64_t)now+(uint32_t)remaining);
         }
         owner->pain.ai_timer=restored[0];owner->pain.animation_lock=restored[1];owner->pain.cooldown=restored[2];
         owner->pain.selected_action=stage->pain[i][3];}
        if(!rf_scene_burning_restored[1]){
            rf_scene_burning_restored[1]=stage->target_uid[i];rf_scene_burning_restored[2]=stage->source_kind[i];
            rf_scene_burning_restored[3]=stage->source_uid[i];rf_scene_burning_restored[4]=stage->records[i].left;
            rf_scene_burning_restored[5]=stage->records[i].phase;memcpy(rf_scene_burning_restored+6,&owner->damage.effects.health,4);
        }
    }
}
#endif
