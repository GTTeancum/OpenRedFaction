#ifndef RF_SCENE_CORPSE_LIFETIME_CHECKPOINT_INCLUDED
#define RF_SCENE_CORPSE_LIFETIME_CHECKPOINT_INCLUDED
/* RFCL1 is an outer companion of the vehicle component. RFNC10 still owns
 * death pose, inventory and actor identity. Include after scene_history_word/
 * put and NPC checkpoint types. No death callback is invoked by this file. */
enum {SCENE_CORPSE_LIFETIME_HEADER=16,SCENE_CORPSE_LIFETIME_ROW=48};
typedef struct scene_corpse_lifetime_saved {
    uint32_t uid,class_id;float age,fade;
    uint32_t flags,object_flags;float health,temperature,class_temperature;
    int32_t motion,action;
} scene_corpse_lifetime_saved;
typedef struct scene_corpse_lifetime_stage {
    uint32_t count,allocated_bytes,admitted;
    scene_corpse_lifetime_saved rows[RF_CORPSE_CAPACITY];
} scene_corpse_lifetime_stage;
static scene_corpse_lifetime_stage scene_corpse_lifetime_pending;
static uint32_t scene_corpse_lifetime_capture_scope;
uint32_t rf_scene_corpse_lifetime_checkpoint[8]; /* captures,queued,applied,rows,firstUID,first fade bits,first age bits,error */

static int scene_corpse_lifetime_values(const scene_corpse_lifetime_saved *r)
{
    if(!r||!r->uid||r->uid==UINT32_MAX||!isfinite(r->age)||r->age<0||
       !isfinite(r->fade)||r->fade<0||r->fade>1||
       (r->flags&~0x4c3u)||(r->object_flags&(2u|0x4000u))||
       !isfinite(r->health)||r->health<0||!isfinite(r->temperature)||
       !isfinite(r->class_temperature)||r->motion<0||r->action<5||r->action>16||
       ((r->flags&1u)&&r->fade<=0))return RF_NOT_FOUND;
    return RF_OK;
}
/* Capture gate also used before RFNC accepts an unsettled fading death.
 * Seek/blend tails, fire/emitters, carried items and separately moving bodies
 * remain unsupported; do not silently discard their state. */
static int scene_corpse_lifetime_read(const rf_corpse_owned *owner,float now,
    scene_corpse_lifetime_saved *r)
{
    const rf_corpse *c;const campaign_npc_body *a;uint32_t slot,k;
    if(!owner||!r||!isfinite(now))return RF_RANGE;c=&owner->corpse;
    if(!c->update.model||c->update.model>campaign_npc_body_count||
       c->update.model>campaign_model_owner_count)return RF_FORMAT;
    slot=c->update.model-1;a=campaign_npc_bodies+slot;
    if(owner->construction!=RF_CORPSE_CONSTRUCT_COMPLETE||c->deletion.lifecycle||
       c->deletion.burn||c->deletion.emitters||c->extra_model||c->update.item_2cc!=-1||
       c->update.emitter_deadline_2ac!=-1||a->damage.effects.health>0||
       !a->registration.view||!(a->view.flags_810&1u)||
       c->uid!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
       c->class_index!=campaign_seeds.items[slot].class_index||
       !campaign_model_owners[slot].owned||!campaign_model_owners[slot].pose||
       !campaign_model_owners[slot].registration.loaded)return RF_NOT_FOUND;
    for(k=0;k<3;k++)if(c->velocity[k]!=0||c->vector_150[k]!=0||
        c->update.position[k]!=campaign_model_owners[slot].position[k])return RF_NOT_FOUND;
    if(memcmp(c->update.basis,campaign_model_owners[slot].basis,36))return RF_NOT_FOUND;
    memset(r,0,sizeof(*r));r->uid=c->uid;r->class_id=c->class_index;
    r->age=now-c->created_seconds;r->fade=c->update.fade.fade_298;
    r->flags=c->update.fade.flags_29c;r->object_flags=c->update.fade.object_flags_7c;
    r->health=c->update.fade.health_34;r->temperature=c->update.value_2b0;
    r->class_temperature=c->update.class_value;r->motion=c->update.motion_2b8;
    r->action=a->death.action_824;return scene_corpse_lifetime_values(r);
}
static void scene_corpse_lifetime_checkpoint_reset(void)
{
    memset(&scene_corpse_lifetime_pending,0,sizeof(scene_corpse_lifetime_pending));
    scene_corpse_lifetime_capture_scope=0;
    memset(rf_scene_corpse_lifetime_checkpoint,0,sizeof(rf_scene_corpse_lifetime_checkpoint));
}
/* Scope only around the ordinary world's RFNC export, whose later RFCL
 * capture must also succeed. Standalone RFNC cannot carry the fade owner. */
static void scene_corpse_lifetime_checkpoint_scope(uint32_t enabled)
{scene_corpse_lifetime_capture_scope=!!enabled;}
static int scene_corpse_lifetime_checkpoint_fading(const rf_corpse_owned *owner)
{
    scene_corpse_lifetime_saved saved;
    if(!scene_corpse_lifetime_capture_scope)return RF_NOT_FOUND;
    return scene_corpse_lifetime_read(owner,(float)((double)campaign_blackout_now*.001),&saved);
}
static int scene_corpse_lifetime_checkpoint_capture(unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,int32_t now,uint32_t *written)
{
    scene_corpse_lifetime_saved rows[RF_CORPSE_CAPACITY];
    const rf_corpse_list_link *link=campaign_live_corpse_head.next,*previous=&campaign_live_corpse_head;
    uint32_t count=0,i,j,mask=0,joined=0;uint64_t size;int status;
    if(!p||!written||inner_bytes>capacity||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    while(link!=&campaign_live_corpse_head) {
        if(!link||count==RF_CORPSE_CAPACITY||link->previous!=previous)return RF_FORMAT;
        for(i=0;i<RF_CORPSE_CAPACITY;i++)if((campaign_live_corpses.pool.active_mask&(1u<<i))&&
            link==&campaign_live_corpses.slots[i].corpse.deletion.corpse_link)break;
        if(i==RF_CORPSE_CAPACITY||(mask&(1u<<i)))return RF_FORMAT;mask|=1u<<i;
        status=scene_corpse_lifetime_read(campaign_live_corpses.slots+i,(float)((double)now*.001),rows+count);
        if(status)return status;
        for(j=0;j<count;j++)if(rows[j].uid==rows[count].uid)return RF_FORMAT;
        ++count;previous=link;link=link->next;
    }
    if(previous!=campaign_live_corpse_head.previous||count!=campaign_live_corpse_count||
       mask!=campaign_live_corpses.pool.active_mask)return RF_FORMAT;
    if(!count){*written=inner_bytes;return RF_OK;}
    /* RFNC permits a frozen dead pose whose presentation allocation never
     * became an owned body. RFCL admission requires complete correspondence;
     * reject that mixed source batch now, instead of writing an unloadable
     * save. Use RFNC's exact dead-pose criterion, not every removed actor. */
    for(i=0;i<campaign_npc_body_count;i++) {
        const campaign_npc_body *a=campaign_npc_bodies+i;
        const rf_campaign_object_record *key;
        if(!a->persistence_registered)continue;
        if(a->persistence_slot>=rf_scene_defeated_actors.count||
           a->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
        key=rf_scene_defeated_actors.items+a->persistence_slot;
        if(!a->registration.view||a->damage.effects.health>0||key->retired)continue;
        for(j=0;j<count;j++)if(rows[j].uid==key->uid)break;
        if(j==count)return RF_NOT_FOUND;
        ++joined;
    }
    if(joined!=count)return RF_NOT_FOUND;
    size=16u+(uint64_t)inner_bytes+(uint64_t)count*48u;
    if(size>capacity||size>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    memmove(p+16,p,inner_bytes);memcpy(p,"RFCL",4);
    scene_history_put(p+4,1);scene_history_put(p+8,inner_bytes);scene_history_put(p+12,count);
    for(i=0;i<count;i++) {
        const scene_corpse_lifetime_saved *r=rows+i;unsigned char *q=p+16+inner_bytes+i*48;
        scene_history_put(q,r->uid);scene_history_put(q+4,r->class_id);
        memcpy(q+8,&r->age,4);memcpy(q+12,&r->fade,4);
        scene_history_put(q+16,r->flags);scene_history_put(q+20,r->object_flags);
        memcpy(q+24,&r->health,4);memcpy(q+28,&r->temperature,4);memcpy(q+32,&r->class_temperature,4);
        scene_history_put(q+36,(uint32_t)r->motion);scene_history_put(q+40,(uint32_t)r->action);scene_history_put(q+44,0);
    }
    *written=(uint32_t)size;++rf_scene_corpse_lifetime_checkpoint[0];return RF_OK;
}
static void scene_corpse_lifetime_checkpoint_close(scene_corpse_lifetime_stage **s)
{if(s&&*s){free(*s);*s=NULL;}}
static int scene_corpse_lifetime_checkpoint_prepare(const unsigned char *p,uint32_t bytes,
    uint32_t budget,scene_corpse_lifetime_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_corpse_lifetime_stage *s;uint32_t count,size,i,j;int status;
    if(!out||*out||!inner||!inner_bytes||(bytes&&!p))return RF_RANGE;
    if(bytes<4||memcmp(p,"RFCL",4)){*inner=p;*inner_bytes=bytes;return RF_OK;}
    if(bytes<16||scene_history_word(p+4)!=1)return RF_FORMAT;
    size=scene_history_word(p+8);count=scene_history_word(p+12);
    if(!count||count>RF_CORPSE_CAPACITY||16u+(uint64_t)size+(uint64_t)count*48u!=bytes)return RF_FORMAT;
    if(sizeof(*s)>budget)return RF_RANGE;s=calloc(1,sizeof(*s));if(!s)return RF_IO;
    s->count=count;s->allocated_bytes=sizeof(*s);
    for(i=0;i<count;i++) {
        const unsigned char *q=p+16+size+i*48;scene_corpse_lifetime_saved *r=s->rows+i;
        r->uid=scene_history_word(q);r->class_id=scene_history_word(q+4);
        memcpy(&r->age,q+8,4);memcpy(&r->fade,q+12,4);r->flags=scene_history_word(q+16);
        r->object_flags=scene_history_word(q+20);memcpy(&r->health,q+24,4);
        memcpy(&r->temperature,q+28,4);memcpy(&r->class_temperature,q+32,4);
        r->motion=(int32_t)scene_history_word(q+36);r->action=(int32_t)scene_history_word(q+40);
        status=scene_corpse_lifetime_values(r);if(status||scene_history_word(q+44))goto malformed;
        for(j=0;j<i;j++)if(s->rows[j].uid==r->uid)goto malformed;
    }
    *inner=p+16;*inner_bytes=size;*out=s;return RF_OK;
malformed:free(s);return RF_FORMAT;
}
/* Join against decoded RFNC before freeing those rows. No registered handles
 * or live mutations are needed. Every serialized body must have its one real
 * dead actor and every serialized dead pose must have a body companion. */
static int scene_corpse_lifetime_checkpoint_admit(scene_corpse_lifetime_stage *s,
    const rf_npc_checkpoint_record *rows,uint32_t count)
{
    uint32_t i,j,found=0;if(!s)return RF_OK;if(count&&!rows)return RF_RANGE;
    for(i=0;i<count;i++)if(rows[i].dead_pose) {
        const rf_npc_checkpoint_record *a=rows+i;
        for(j=0;j<s->count;j++)if(s->rows[j].uid==a->uid)break;
        if(j==s->count||a->retired||a->health>0||!(a->death_flags_810&1u)||
           a->class_id!=s->rows[j].class_id||a->death_action!=s->rows[j].action||
           !a->animation_present||a->playback.completion.active.count!=1||
           a->playback.completion.active.slots[0].motion!=s->rows[j].motion)return RF_FORMAT;
        ++found;
    }
    if(found!=s->count)return RF_FORMAT;s->admitted=1;return RF_OK;
}
/* Assignment-only transaction hook, before deferred corpse reconstruction.
 * NULL clears stale pending state when loading a legacy save. */
static void scene_corpse_lifetime_checkpoint_assign(const scene_corpse_lifetime_stage *s)
{
    memset(&scene_corpse_lifetime_pending,0,sizeof(scene_corpse_lifetime_pending));
    if(s&&s->admitted){scene_corpse_lifetime_pending=*s;++rf_scene_corpse_lifetime_checkpoint[1];}
}
/* Call once after ALL deferred corpse creations, before their first update.
 * Validate the complete destination first. Constructor retention may have
 * chosen a different oldest body; overwrite each scalar and list order only
 * after the whole batch exists. Never restart the one-second timer. */
static int scene_corpse_lifetime_checkpoint_apply(int32_t now)
{
    scene_corpse_lifetime_stage *s=&scene_corpse_lifetime_pending;
    rf_corpse *matches[RF_CORPSE_CAPACITY];uint32_t i,j,found;float created[RF_CORPSE_CAPACITY];
    if(!s->admitted)return RF_OK;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(campaign_live_corpse_count!=s->count)goto malformed;
    for(i=0;i<s->count;i++) {
        found=0;matches[i]=NULL;created[i]=(float)((double)now*.001)-s->rows[i].age;
        if(!isfinite(created[i]))goto malformed;
        for(j=0;j<RF_CORPSE_CAPACITY;j++)if(campaign_live_corpses.pool.active_mask&(1u<<j)) {
            rf_corpse_owned *o=campaign_live_corpses.slots+j;rf_corpse *c=&o->corpse;
            if(c->uid!=s->rows[i].uid)continue;
            if(found++||o->construction!=RF_CORPSE_CONSTRUCT_COMPLETE||c->deletion.lifecycle||
               c->class_index!=s->rows[i].class_id||c->update.motion_2b8!=s->rows[i].motion||
               c->deletion.burn||c->deletion.emitters||c->extra_model||c->update.item_2cc!=-1)goto malformed;
            matches[i]=c;
        }
        if(!found)goto malformed;
    }
    for(i=0;i<s->count;i++) {
        rf_corpse *c=matches[i];const scene_corpse_lifetime_saved *r=s->rows+i;
        c->created_seconds=created[i];c->update.fade.fade_298=r->fade;
        c->update.fade.flags_29c=r->flags;c->update.fade.object_flags_7c=r->object_flags;
        c->update.fade.health_34=r->health;c->update.value_2b0=r->temperature;c->update.class_value=r->class_temperature;
        c->deletion.corpse_link.previous=i?&matches[i-1]->deletion.corpse_link:&campaign_live_corpse_head;
        c->deletion.corpse_link.next=i+1<s->count?&matches[i+1]->deletion.corpse_link:&campaign_live_corpse_head;
    }
    campaign_live_corpse_head.next=&matches[0]->deletion.corpse_link;
    campaign_live_corpse_head.previous=&matches[s->count-1]->deletion.corpse_link;
    ++rf_scene_corpse_lifetime_checkpoint[2];rf_scene_corpse_lifetime_checkpoint[3]=s->count;
    rf_scene_corpse_lifetime_checkpoint[4]=s->rows[0].uid;
    memcpy(rf_scene_corpse_lifetime_checkpoint+5,&s->rows[0].fade,4);
    memcpy(rf_scene_corpse_lifetime_checkpoint+6,&s->rows[0].age,4);
    memset(s,0,sizeof(*s));return RF_OK;
malformed:rf_scene_corpse_lifetime_checkpoint[7]=RF_FORMAT;return RF_FORMAT;
}
#endif
