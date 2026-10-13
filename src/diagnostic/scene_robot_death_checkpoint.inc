#ifndef RF_SCENE_ROBOT_DEATH_CHECKPOINT_INCLUDED
#define RF_SCENE_ROBOT_DEATH_CHECKPOINT_INCLUDED
/* RFAP13 kind27: queued-only robot finalizer. UINT64 sequence and high-water
 * preserve FIFO across new chain admissions. Handles, slots and optional
 * presentation indices are destination bindings, never portable identities. */
static int scene_robot_death_checkpoint_capture(unsigned char *rows,uint32_t *count)
{
    uint32_t n=0;uint64_t previous=0;int status;
    if(!count)return RF_RANGE;
    if(scene_robot_death_checkpoint_busy())return RF_NOT_FOUND;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const scene_robot_death_receipt *r=&campaign_npc_bodies[i].robot_death;
        if(r->state>2)return RF_FORMAT;
        if(r->state!=1)continue;
        status=scene_robot_death_checkpoint_live(i);if(status)return status;
        if(!scene_robot_death_sequence||scene_robot_death_sequence==UINT64_MAX||
           r->sequence>scene_robot_death_sequence)return RF_FORMAT;
        if(++n>SCENE_ROBOT_DEATH_CHECKPOINT_CAPACITY)return RF_RANGE;
        for(uint32_t j=0;j<i;j++)if(campaign_npc_bodies[j].robot_death.state==1&&
            (campaign_npc_bodies[j].robot_death.sequence==r->sequence||
             campaign_npc_bodies[j].robot_death.uid==r->uid||
             campaign_npc_bodies[j].robot_death.handle==r->handle))return RF_FORMAT;
    }
    *count=n;if(!rows)return RF_OK;
    for(uint32_t i=0;i<n;i++){
        const scene_robot_death_receipt *first=NULL;unsigned char *w=rows+112*i;
        for(uint32_t j=0;j<campaign_npc_body_count;j++){
            const scene_robot_death_receipt *r=&campaign_npc_bodies[j].robot_death;
            if(r->state==1&&r->sequence>previous&&(!first||r->sequence<first->sequence))first=r;
        }
        if(!first)return RF_FORMAT;previous=first->sequence;
        checkpoint_put(w,27);checkpoint_put(w+4,i);checkpoint_put(w+8,first->uid);
        checkpoint_put(w+12,first->profile);checkpoint_put(w+16,first->class_index);
        checkpoint_put(w+20,(uint32_t)first->sequence);checkpoint_put(w+24,(uint32_t)(first->sequence>>32));
        checkpoint_put(w+28,(uint32_t)scene_robot_death_sequence);checkpoint_put(w+32,(uint32_t)(scene_robot_death_sequence>>32));
        for(uint32_t k=0;k<3;k++)checkpoint_put_float(w+36+k*4,first->position[k]);
        checkpoint_put_float(w+48,first->health);checkpoint_put_float(w+52,first->damage);checkpoint_put_float(w+56,first->radius);
    }
    return RF_OK;
}
static int scene_robot_death_checkpoint_read(scene_robot_death_checkpoint_stage *stage,const unsigned char *w)
{
    scene_robot_death_checkpoint_entry next={0};scene_robot_death_receipt *r=&next.receipt;
    uint32_t ordinal=checkpoint_u32(w+4),found=0;uint64_t sequence;int status;
    if(!stage||checkpoint_u32(w)!=27||ordinal!=stage->count||ordinal>=SCENE_ROBOT_DEATH_CHECKPOINT_CAPACITY)return RF_FORMAT;
    for(uint32_t k=60;k<112;k++)if(w[k])return RF_FORMAT;
    sequence=(uint64_t)checkpoint_u32(w+28)|((uint64_t)checkpoint_u32(w+32)<<32);
    r->state=1;r->uid=checkpoint_u32(w+8);r->profile=checkpoint_u32(w+12);r->class_index=checkpoint_u32(w+16);
    r->sequence=(uint64_t)checkpoint_u32(w+20)|((uint64_t)checkpoint_u32(w+24)<<32);
    if(!sequence||sequence==UINT64_MAX||!r->sequence||r->sequence>sequence||
       (stage->count&&(sequence!=stage->sequence||stage->entries[stage->count-1].receipt.sequence>=r->sequence))||
       r->profile>=SCENE_ROBOT_DEATH_PROFILE_COUNT)return RF_FORMAT;
    for(uint32_t k=0;k<3;k++)r->position[k]=checkpoint_float(w+36+k*4);
    r->health=checkpoint_float(w+48);r->damage=checkpoint_float(w+52);r->radius=checkpoint_float(w+56);
    r->clip=scene_robot_death_profiles[r->profile].clip;r->foley_group=scene_robot_death_profiles[r->profile].foley_group;
    status=scene_robot_death_checkpoint_shape(r);if(status)return status;
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||!campaign_seeds.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_FORMAT;
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==r->uid){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(found++||scene_robot_death_profile_at(i)!=(int32_t)r->profile||
           campaign_seeds.items[i].class_index!=r->class_index||
           !scene_robot_death_identity(i,o->registration.handle)||!o->body.allocated_bytes||
           !o->persistence_registered||o->persistence_slot>=rf_scene_defeated_actors.count||
           o->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS||
           rf_scene_defeated_actors.items[o->persistence_slot].uid!=r->uid||
           rf_scene_defeated_actors.items[o->persistence_slot].retired||
           rf_scene_defeated_actors.items[o->persistence_slot].level>=rf_scene_defeated_actors.level_count||
           rf_scene_defeated_actors.items[o->persistence_slot].level>=RF_CAMPAIGN_PICKUP_LEVELS||
           !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[
               rf_scene_defeated_actors.items[o->persistence_slot].level],campaign_current_level))return RF_FORMAT;
        next.slot=i;next.persistence_before=r->persistence=o->persistence_slot;r->handle=o->registration.handle;
    }
    if(found!=1)return RF_FORMAT;
    for(uint32_t i=0;i<stage->count;i++)if(stage->entries[i].receipt.uid==r->uid||
        stage->entries[i].receipt.handle==r->handle||stage->entries[i].slot==next.slot)return RF_FORMAT;
    stage->sequence=sequence;stage->entries[stage->count++]=next;return RF_OK;
}
static int scene_robot_death_checkpoint_admit(const scene_ai_projectile_checkpoint_stage *projectiles,
    scene_npc_checkpoint_restore_stage *npcs)
{
    const scene_robot_death_checkpoint_stage *stage=projectiles?&projectiles->robot_death:NULL;
    uint32_t markers=0;
    if(!npcs)return RF_RANGE;
    for(uint32_t i=0;i<npcs->count;i++)if(npcs->entries[i].saved.dead_pose==2)++markers;
    if(markers!=(stage?stage->count:0))return RF_FORMAT;
    if(!markers)return RF_OK;
    if(!npcs->fresh_boot||!npcs->flight_profile||!stage||stage->count>SCENE_ROBOT_DEATH_CHECKPOINT_CAPACITY)return RF_FORMAT;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_robot_death_checkpoint_entry *source=stage->entries+i;
        const scene_robot_death_receipt *r=&source->receipt;scene_npc_checkpoint_restore_entry *e=NULL;
        const campaign_npc_body *o;uint32_t found=0;
        if(scene_robot_death_checkpoint_shape(r)||source->slot>=campaign_npc_body_count)return RF_FORMAT;
        o=scene_robot_death_identity(source->slot,r->handle);
        if(!o||!o->body.allocated_bytes||!o->persistence_registered||o->persistence_slot!=source->persistence_before||
           scene_robot_death_profile_at(source->slot)!=(int32_t)r->profile||
           campaign_seeds.items[source->slot].class_index!=r->class_index||
           (uint32_t)campaign_seeds.records.items[source->slot].record.uid!=r->uid||
           !campaign_model_owners||source->slot>=campaign_model_owner_count||campaign_model_owners[source->slot].owned)return RF_FORMAT;
        for(uint32_t j=0;j<npcs->count;j++)if(npcs->entries[j].saved.uid==r->uid){
            if(found++)return RF_FORMAT;e=npcs->entries+j;
        }
        if(found!=1||!e||e->slot!=source->slot||e->handle!=r->handle||!e->registered||
           e->saved.class_id!=r->class_index||e->saved.retired||e->saved.dead_pose!=2||
           e->saved.death_action!=-1||!(e->saved.death_flags_810&1u)||e->saved.health>0||
           e->saved.animation_present||e->unowned_motion_mask!=SCENE_ROBOT_DEATH_CHECKPOINT_UNJOINED)return RF_FORMAT;
    }
    /* Validation above is all-or-nothing, including duplicates/order from
     * read. Only a complete one-to-one join discharges NPC obligations. */
    for(uint32_t i=0;i<npcs->count;i++)if(npcs->entries[i].saved.dead_pose==2)npcs->entries[i].unowned_motion_mask&=~SCENE_ROBOT_DEATH_CHECKPOINT_UNJOINED;
    return RF_OK;
}
/* Final native-close success only, after history rebinding and NPC finish.
 * No fallible allocation, gameplay callback, radial work or retirement here.
 * The existing outer drain consumes state1->2 before removal and blast, and
 * retains its epoch guard/fault latch if that work is interrupted. */
static void scene_robot_death_checkpoint_finish(const scene_ai_projectile_checkpoint_stage *projectiles)
{
    const scene_robot_death_checkpoint_stage *stage=projectiles?&projectiles->robot_death:NULL;
    scene_robot_death_reset();if(!stage)return;
    scene_robot_death_sequence=stage->sequence;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_robot_death_checkpoint_entry *e=stage->entries+i;
        campaign_npc_body *o=campaign_npc_bodies+e->slot;
        o->robot_death=e->receipt;o->robot_death.persistence=o->persistence_slot;
    }
    rf_scene_robot_death[10]=stage->count;
}
#endif
