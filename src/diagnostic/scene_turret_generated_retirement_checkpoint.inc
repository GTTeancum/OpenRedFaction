#ifndef RF_SCENE_TURRET_GENERATED_RETIREMENT_CHECKPOINT_INCLUDED
#define RF_SCENE_TURRET_GENERATED_RETIREMENT_CHECKPOINT_INCLUDED
/* RFTU3 is an alternative to RFTU2, never nested inside it. Include after
 * generated checkpoint primitives and NPC checkpoint capture. Runtime removal
 * remains defined later in scene.c and supplies these bounded sidecar APIs. */
static int scene_turret_generated_retirement_read(uint32_t,uint32_t *);
static void scene_turret_generated_retirement_assign(uint32_t,uint32_t);
enum {SCENE_TURRET_RETIREMENT_ROW=224};

static uint32_t scene_turret_retirement_checkpoint_version3(
    const scene_turret_generated_checkpoint_stage *stage)
{
    if(stage)for(uint32_t i=0;i<stage->count;i++)if(stage->rows[i].removal_reason)return 1;
    return 0;
}

/* Shared with the parent's paused RFNC snapshot cross-check. An absent base
 * registration is accepted ONLY with its real removal marker, authored slot,
 * UID, current level and retired persistence key. No stale-handle lookup. */
static int scene_turret_retirement_checkpoint_capture_base(void *context,uint32_t uid,
    scene_turret_generated_base_saved *out)
{
    uint32_t i,reason;campaign_npc_body *base;const rf_campaign_object_record *key;int status;
    (void)context;if(!out||uid==UINT32_MAX)return RF_RANGE;
    for(i=0;i<scene_turret_generated_count;i++)if(scene_turret_generated[i].key.base_uid==uid)break;
    if(i==scene_turret_generated_count)return RF_NOT_FOUND;
    const scene_turret_generated_binding *b=scene_turret_generated+i;
    status=scene_turret_generated_retirement_read(i,&reason);if(status)return status;
    if(reason!=1)return scene_turret_generated_checkpoint_live_base(NULL,uid,out);
    if(!campaign_npc_bodies||b->base_slot>=campaign_npc_body_count||!scene_turret_generated_is_base(b->base_slot)||
       (uint32_t)campaign_seeds.records.items[b->base_slot].record.uid!=uid)return RF_FORMAT;
    base=campaign_npc_bodies+b->base_slot;
    if(base->registration.view||!base->persistence_registered||base->persistence_slot>=rf_scene_defeated_actors.count||
       base->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_NOT_FOUND;
    key=rf_scene_defeated_actors.items+base->persistence_slot;
    if(key->uid!=uid||key->retired!=1||key->level>=rf_scene_defeated_actors.level_count||
       key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
       !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level))return RF_FORMAT;
    if((base->object_flags&(2u|0x4000u))!=(2u|0x4000u))return RF_FORMAT;
    out->health=base->damage.effects.health;out->flags_7d0=base->view.flags_7d0;
    out->flags_810=base->view.flags_810;out->retired=1;return RF_OK;
}

static int scene_turret_retirement_checkpoint_valid(const scene_turret_generated_saved *r,uint32_t frame)
{
    const scene_turret_saved *v=&r->value;int status;
    if(!r->removal_reason)return scene_turret_generated_checkpoint_valid(r,frame);
    status=scene_turret_generated_checkpoint_valid_common(r,frame);if(status)return status;
    if(r->removal_reason>2||r->base.retired!=(r->removal_reason==1)||
       (v->flags&(2u|0x4000u))!=(2u|0x4000u)||r->target_role||v->target_uid!=UINT32_MAX||
       v->remaining||v->burst||(r->base.flags_7d0&1u)||(r->base.flags_810&0x10u)||
       (r->removal_reason==1&&r->base.health>0))return RF_FORMAT;
    return RF_OK;
}

static int scene_turret_retirement_checkpoint_capture_row(uint32_t index,uint32_t frame,
    scene_turret_generated_saved *r)
{
    uint32_t reason;scene_turret_generated_binding *b;scene_turret_owner *o;int status;
    status=scene_turret_generated_retirement_read(index,&reason);if(status)return status;
    if(!reason)return scene_turret_generated_checkpoint_capture_row(index,frame,r);
    status=scene_turret_generated_checkpoint_capture_head(index,frame,r);if(status)return status;
    b=scene_turret_generated+index;o=scene_turrets+r->index;
    if(o->view.linked_handle!=-1)return RF_FORMAT;
    r->removal_reason=reason;
    status=scene_turret_retirement_checkpoint_capture_base(NULL,b->key.base_uid,&r->base);if(status)return status;
    return scene_turret_retirement_checkpoint_valid(r,frame);
}

/* Caller proves every base agrees with the already captured RFNC slice using
 * capture_base first. No caller bytes change until ALL generated rows admit. */
static int scene_turret_retirement_checkpoint_capture(unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,uint32_t frame,uint32_t budget,uint32_t *written)
{
    uint32_t i,reason,removed=0,n=scene_turret_generated_count;uint64_t required,allocated;
    scene_turret_generated_saved *rows;int status;
    if(!p||!written||inner_bytes>capacity||frame==UINT32_MAX)return RF_RANGE;
    for(i=0;i<n;i++){status=scene_turret_generated_retirement_read(i,&reason);if(status)return status;removed|=reason;}
    if(!removed)return scene_turret_generated_checkpoint_capture(p,capacity,inner_bytes,frame,budget,written);
    required=16+(uint64_t)inner_bytes+(uint64_t)n*SCENE_TURRET_RETIREMENT_ROW;
    allocated=(uint64_t)n*sizeof(*rows);
    if(!n||n>SCENE_TURRET_OWNER_LIMIT||required>capacity||required>RF_CHECKPOINT_FILE_MAX||allocated>budget)return RF_RANGE;
    if(inner_bytes>=8&&!memcmp(p,"RFTU",4)&&scene_history_word(p+4)!=1)return RF_FORMAT;
    rows=calloc(n,sizeof(*rows));if(!rows)return RF_IO;
    for(i=0;i<n;i++){status=scene_turret_retirement_checkpoint_capture_row(i,frame,rows+i);if(status){free(rows);return status;}}
    memmove(p+16,p,inner_bytes);memcpy(p,"RFTU",4);scene_history_put(p+4,3);
    scene_history_put(p+8,inner_bytes);scene_history_put(p+12,n);
    for(i=0;i<n;i++) {
        unsigned char *q=p+16+inner_bytes+i*SCENE_TURRET_RETIREMENT_ROW;
        scene_turret_generated_checkpoint_encode(q,rows+i);
        scene_history_put(q+216,rows[i].removal_reason);scene_history_put(q+220,rows[i].base.retired);
    }
    free(rows);*written=(uint32_t)required;return RF_OK;
}

static int scene_turret_retirement_checkpoint_prepare(const unsigned char *p,uint32_t bytes,
    uint32_t frame,uint32_t budget,scene_turret_generated_checkpoint_stage **out,
    const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_turret_generated_checkpoint_stage *stage;uint32_t n,size,i,j,handle,removed=0;
    uint64_t required,allocated;int status;
    if(!out||*out||!inner||!inner_bytes||frame==UINT32_MAX||(bytes&&!p))return RF_RANGE;
    if(bytes<8||memcmp(p,"RFTU",4)||scene_history_word(p+4)!=3)
        return scene_turret_generated_checkpoint_prepare(p,bytes,frame,budget,out,inner,inner_bytes);
    if(bytes<16)return RF_FORMAT;
    n=scene_history_word(p+12);size=scene_history_word(p+8);
    required=16+(uint64_t)size+(uint64_t)n*SCENE_TURRET_RETIREMENT_ROW;
    if(!n||n>SCENE_TURRET_OWNER_LIMIT||required!=bytes||required>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
    if(size>=8&&!memcmp(p+16,"RFTU",4)&&scene_history_word(p+20)!=1)return RF_FORMAT;
    allocated=sizeof(*stage)+(uint64_t)n*sizeof(*stage->rows);
    if(n!=scene_turret_generated_count||allocated>budget||allocated>UINT32_MAX)return RF_RANGE;
    stage=calloc(1,(size_t)allocated);if(!stage)return RF_IO;
    stage->count=n;stage->frame=frame;stage->allocated_bytes=(uint32_t)allocated;
    for(i=0;i<n;i++) {
        scene_turret_generated_saved *r=stage->rows+i;scene_turret_owner *o;scene_turret_generated_binding *b;
        const unsigned char *q=p+16+size+i*SCENE_TURRET_RETIREMENT_ROW;
        status=scene_turret_generated_checkpoint_decode(q,r);if(status)goto fail;
        r->removal_reason=scene_history_word(q+216);r->base.retired=scene_history_word(q+220);
        removed|=r->removal_reason;
        for(j=0;j<i;j++)if(stage->rows[j].key.base_uid==r->key.base_uid){status=RF_FORMAT;goto fail;}
        status=scene_turret_generated_resolve(r->key,&handle);if(status)goto fail;
        o=scene_turret_lookup(handle);b=scene_turret_generated_find(handle);
        if(!o||!b){status=RF_NOT_FOUND;goto fail;}
        r->index=(uint32_t)(o-scene_turrets);r->value.handle=handle;r->base_handle=b->base_handle;
        status=scene_turret_retirement_checkpoint_valid(r,frame);if(status)goto fail;
        status=scene_turret_generated_checkpoint_target(r->value.target_uid,1,&r->target_role,&r->value.target_handle);if(status)goto fail;
        if(r->value.target_handle==handle){status=RF_FORMAT;goto fail;}
    }
    if(!removed){status=RF_FORMAT;goto fail;}
    *out=stage;*inner=p+16;*inner_bytes=size;return RF_OK;
fail:scene_turret_generated_checkpoint_close(&stage);return status;
}

static int scene_turret_retirement_checkpoint_admit(const scene_turret_generated_checkpoint_stage *stage,
    scene_turret_generated_base_reader read_base,scene_turret_generated_target_reader read_target,void *context)
{
    uint32_t i,handle,target,role,alive;int status;
    if(!scene_turret_retirement_checkpoint_version3(stage))
        return scene_turret_generated_checkpoint_admit(stage,read_base,read_target,context);
    if(!read_base||!read_target||stage->count!=scene_turret_generated_count)return RF_RANGE;
    for(i=0;i<stage->count;i++) {
        const scene_turret_generated_saved *r=stage->rows+i;scene_turret_generated_binding *b;
        scene_turret_generated_base_saved base={0};
        status=scene_turret_retirement_checkpoint_valid(r,stage->frame);if(status)return status;
        status=scene_turret_generated_resolve(r->key,&handle);if(status)return status;
        b=scene_turret_generated_find(handle);
        if(!b||handle!=r->value.handle||b->base_handle!=r->base_handle)return RF_NOT_FOUND;
        /* A fresh registered base is the normal destination. A previously
         * removed base can only accept another admitted removed-base row;
         * never authorize resurrection of its freed physics owner. */
        if(!scene_turret_generated_base(b)) {
            if(r->removal_reason!=1)return RF_NOT_FOUND;
            status=scene_turret_retirement_checkpoint_capture_base(NULL,r->key.base_uid,&base);if(status)return status;
            if(!base.retired)return RF_NOT_FOUND;
        }
        role=r->target_role;
        status=scene_turret_generated_checkpoint_target(r->value.target_uid,1,&role,&target);if(status)return status;
        if(target!=r->value.target_handle||target==handle)return RF_NOT_FOUND;
        if(role||r->value.target_uid!=UINT32_MAX) {
            alive=UINT32_MAX;status=read_target(context,role,r->value.target_uid,&alive);if(status)return status;
            if(alive!=1)return RF_FORMAT;
        }
        base.retired=UINT32_MAX;
        status=read_base(context,r->key.base_uid,&base);if(status)return status;
        if(base.retired!=r->base.retired||base.health!=r->base.health||
           (base.flags_810&1u)!=(r->base.flags_810&1u))return RF_FORMAT;
    }
    return RF_OK;
}

/* Called after NPC publication, never after replaying Remove_Object. This
 * restores the explicit terminal marker without damage or unregister calls.
 * Always use this dispatcher for RFTU2 too, to clear stale head-only markers
 * when an earlier active save passes the ordinary NPC admission policy. */
static void scene_turret_retirement_checkpoint_assign(const scene_turret_generated_checkpoint_stage *stage)
{
    if(!stage)return;
    if(!scene_turret_retirement_checkpoint_version3(stage))scene_turret_generated_checkpoint_assign(stage);
    else for(uint32_t i=0;i<stage->count;i++) {
        const scene_turret_generated_saved *r=stage->rows+i;
        scene_turret_generated_checkpoint_assign_row(r,stage->frame,
            r->removal_reason||r->value.dead?-1:(int32_t)r->base_handle);
    }
    for(uint32_t i=0;i<stage->count;i++) {
        const scene_turret_generated_saved *r=stage->rows+i;
        scene_turret_generated_binding *b=scene_turret_generated_find(r->value.handle);
        scene_turret_generated_retirement_assign((uint32_t)(b-scene_turret_generated),r->removal_reason);
    }
}
#endif
