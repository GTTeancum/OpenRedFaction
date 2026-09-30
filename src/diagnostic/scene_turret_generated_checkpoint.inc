/* RFTU2 generated-head companion. Include after generated/ordinary turret
 * checkpoint helpers. Not wired by this include. RFTU2 wraps an unchanged
 * authored-only RFTU1 (or vehicle payload); version1 readers remain unchanged.
 * Parent supplies pending NPC facts during admission, before any assignment. */
enum {SCENE_TURRET_GENERATED_CHECKPOINT_HEADER=16,
      SCENE_TURRET_GENERATED_CHECKPOINT_ROW=216};
typedef struct scene_turret_generated_base_saved {
    float health;uint32_t flags_7d0,flags_810;
    uint32_t retired; /* Admission-only NPC fact; not added to the wire. */
} scene_turret_generated_base_saved;
typedef struct scene_turret_generated_saved {
    scene_turret_generated_key key;
    scene_turret_saved value;
    scene_turret_generated_base_saved base;
    uint32_t death_dispatched,target_role,head_flags_7d0,index,base_handle;
} scene_turret_generated_saved;
typedef struct scene_turret_generated_checkpoint_stage {
    uint32_t count,frame,allocated_bytes;
    scene_turret_generated_saved rows[];
} scene_turret_generated_checkpoint_stage;
/* Return the base facts from the SAME pending NPC checkpoint transaction.
 * Capture may use the provided live callback. Restore supplies pending health
 * and flags_810 death bit; flags_7d0 is ignored during admission because this
 * companion retains base AI flags missing from the generic NPC codec. Restore
 * must not substitute pre-load live health. The callback must explicitly fill
 * retired from the pending NPC row; unregistered parents are unsupported.
 * No mutation/handle allocation here. */
typedef int (*scene_turret_generated_base_reader)(void *,uint32_t,
    scene_turret_generated_base_saved *);
/* Pending-world target admission, separate from fresh-handle resolution.
 * role0: UID0 player, otherwise authored NPC/vehicle/turret UID. role1: generated
 * head identified by its base UID, looked up in the pending RFTU2 stage.
 * Return RF_OK with alive exactly0/1; unknown identities return an error.
 * Dead, retired, removed or hidden saved owners must return alive0. None is
 * handled locally and never passed to this callback. Never use fresh-live
 * health as a substitute for any pending saved owner state. */
typedef int (*scene_turret_generated_target_reader)(void *,uint32_t,uint32_t,uint32_t *);
static void scene_turret_generated_checkpoint_close(scene_turret_generated_checkpoint_stage **stage)
{if(stage && *stage){free(*stage);*stage=NULL;}}
static int scene_turret_generated_checkpoint_inner(const unsigned char *p,uint32_t bytes,
    const unsigned char **inner,uint32_t *inner_bytes,uint32_t *count)
{
    uint32_t n,size;uint64_t required;
    if(!inner || !inner_bytes || !count || (bytes && !p))return RF_RANGE;
    if(bytes<4 || memcmp(p,"RFTU",4)){
        *inner=p;*inner_bytes=bytes;*count=0;return RF_OK;}
    if(bytes<8)return RF_FORMAT;
    if(scene_history_word(p+4)==1){*inner=p;*inner_bytes=bytes;*count=0;return RF_OK;}
    if(scene_history_word(p+4)!=2 || bytes<SCENE_TURRET_GENERATED_CHECKPOINT_HEADER)return RF_FORMAT;
    n=scene_history_word(p+12);size=scene_history_word(p+8);
    required=SCENE_TURRET_GENERATED_CHECKPOINT_HEADER+(uint64_t)size+
        (uint64_t)n*SCENE_TURRET_GENERATED_CHECKPOINT_ROW;
    if(!n || n>SCENE_TURRET_OWNER_LIMIT || required!=bytes || required>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
    /* At most one companion layer; the inner codec admits vehicle/RFTU1. */
    if(size>=8 && !memcmp(p+16,"RFTU",4) && scene_history_word(p+20)!=1)return RF_FORMAT;
    *inner=p+16;*inner_bytes=size;*count=n;return RF_OK;
}
static int scene_turret_generated_checkpoint_live_base(void *context,uint32_t uid,
    scene_turret_generated_base_saved *out)
{
    uint32_t i;(void)context;if(!out)return RF_RANGE;
    for(i=0;i<scene_turret_generated_count;i++)if(scene_turret_generated[i].key.base_uid==uid) {
        campaign_npc_body *base=scene_turret_generated_base(scene_turret_generated+i);
        if(!base)return RF_NOT_FOUND;
        out->health=base->damage.effects.health;out->flags_7d0=base->view.flags_7d0;
        out->flags_810=base->view.flags_810;out->retired=0;return RF_OK;
    }
    return RF_NOT_FOUND;
}
/* target_role0 uses the existing authored UID/player/none convention. Role1
 * means a generated AutoHead keyed by target_uid=its base UID. Decode checks
 * role BEFORE UINT_MAX so malformed generated-none keys cannot become none. */
static int scene_turret_generated_checkpoint_target(uint32_t value,uint32_t decode,
    uint32_t *role,uint32_t *out)
{
    scene_turret_generated_key key;int status;
    if(!role || !out || decode>1)return RF_RANGE;
    if(decode) {
        if(*role==0)return scene_turret_checkpoint_target(value,1,out);
        if(*role!=SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD || value==UINT32_MAX)return RF_FORMAT;
        key.base_uid=value;key.role=*role;return scene_turret_generated_resolve(key,out);
    }
    status=scene_turret_generated_identity(value,&key);
    if(!status){*role=key.role;*out=key.base_uid;return RF_OK;}
    if(status!=RF_NOT_FOUND)return status;
    *role=0;return scene_turret_checkpoint_target(value,0,out);
}
/* Independent saved mount is retained, not replaced by a newly loaded base
 * basis. It must be a finite right-handed orthonormal frame. */
static int scene_turret_generated_checkpoint_valid(const scene_turret_generated_saved *row,uint32_t frame)
{
    const scene_turret_saved *v=&row->value;scene_turret_generated_binding *binding;
    const scene_turret_owner *o;const rf_entity_seed_class *c;float local[9],basis[9],cross[3],dot;
    uint32_t i,j;int status;
    if(row->index>=scene_turret_count || !scene_turret_combat_ready || frame==UINT32_MAX)return RF_RANGE;
    o=scene_turrets+row->index;binding=scene_turret_generated_find(o->registration.handle);
    if(!binding || row->key.role!=SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD || row->key.base_uid==UINT32_MAX ||
       row->key.base_uid!=binding->key.base_uid || scene_turret_lookup(o->registration.handle)!=o ||
       scene_turret_combat_states[row->index].handle!=o->registration.handle ||
       o->view.occupant_count || o->view.weapon_owner || o->damage.effects.burn ||
       (o->view.linked_handle!=-1 && o->view.linked_handle!=(int32_t)binding->base_handle) ||
       v->uid!=UINT32_MAX || o->uid!=UINT32_MAX || v->class_index!=o->class_index ||
       o->class_index>=campaign_seeds.class_count || row->death_dispatched>2 || row->target_role>1 || !(row->head_flags_7d0&0x100000u) ||
       v->dead>1 || !isfinite(v->health) || fabsf(v->health)>1e7f ||
       !isfinite(v->armor) || v->armor<0 || v->armor>1e7f ||
       v->dead!=(v->health<=0) || !!(v->flags_810&1u)!=v->dead || !(v->flags&0x100u) ||
       !!row->death_dispatched!=v->dead ||
       row->base.retired || !isfinite(row->base.health) || fabsf(row->base.health)>1e7f ||
       !!(row->base.flags_810&1u)!=(row->base.health<=0) ||
       (row->base.health<=0 && !v->dead) ||
       v->action< -1 || v->action>13 || v->combat_action< -1 || v->combat_action>13 ||
       v->primary!=scene_turret_vauss_id || v->secondary!=-1 ||
       v->remaining>36000 || v->remaining>UINT32_MAX-frame || v->burst>scene_turret_primary.burst_count ||
       (!row->target_role && v->target_uid==UINT32_MAX && (v->remaining || v->burst)) ||
       (row->target_role && v->target_uid==UINT32_MAX) ||
       (v->dead && (row->target_role || v->target_uid!=UINT32_MAX || v->remaining || v->burst)))return RF_FORMAT;
    c=campaign_seeds.classes+o->class_index;
    for(i=0;i<3;i++)if(!isfinite(v->position[i]) || !isfinite(v->angles[i]) ||
        v->angles[i]<c->eye_limits.minimum[i]-.0001f || v->angles[i]>c->eye_limits.maximum[i]+.0001f ||
        !isfinite(v->angular.velocity[i]) || !isfinite(v->angular.force[i]))return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(v->basis[i]) || !isfinite(v->mount[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=0;j<3;j++) {
        dot=v->mount[i*3]*v->mount[j*3]+v->mount[i*3+1]*v->mount[j*3+1]+v->mount[i*3+2]*v->mount[j*3+2];
        if(fabsf(dot-(i==j?1.f:0.f))>.002f)return RF_FORMAT;
    }
    cross[0]=v->mount[1]*v->mount[5]-v->mount[2]*v->mount[4];
    cross[1]=v->mount[2]*v->mount[3]-v->mount[0]*v->mount[5];
    cross[2]=v->mount[0]*v->mount[4]-v->mount[1]*v->mount[3];
    if(cross[0]*v->mount[6]+cross[1]*v->mount[7]+cross[2]*v->mount[8]<.998f)return RF_FORMAT;
    status=rf_look_orientation(v->angles,local);if(status)return status;
    for(i=0;i<3;i++)scene_turret_world_vector(v->mount,local+i*3,basis+i*3);
    for(i=0;i<9;i++)if(fabsf(v->basis[i]-basis[i])>.0001f)return RF_FORMAT;
    return RF_OK;
}
static int scene_turret_generated_checkpoint_capture_row(uint32_t binding_index,uint32_t frame,
    scene_turret_generated_saved *row)
{
    scene_turret_generated_binding *b;scene_turret_owner *o;scene_turret_combat_state *s;
    scene_turret_saved *v;int status;
    if(!row || binding_index>=scene_turret_generated_count)return RF_RANGE;
    b=scene_turret_generated+binding_index;o=scene_turret_lookup(b->head_handle);if(!o)return RF_NOT_FOUND;
    if(o->damage.effects.burn || o->view.occupant_count || o->view.weapon_owner ||
       o->view.linked_handle!=(o->dead?-1:(int32_t)b->base_handle))return RF_NOT_FOUND;
    if(o->model!=(o->dead?o->dead_model:o->live_model))return RF_FORMAT;
    memset(row,0,sizeof(*row));row->index=(uint32_t)(o-scene_turrets);row->key=b->key;
    row->death_dispatched=b->death_dispatched;row->base_handle=b->base_handle;row->head_flags_7d0=o->view.flags_7d0;
    s=scene_turret_combat_states+row->index;v=&row->value;
    v->uid=UINT32_MAX;v->class_index=o->class_index;v->dead=o->dead;
    v->health=o->damage.effects.health;v->armor=o->damage.effects.armor;
    v->flags=o->view.flags_7c;v->flags_810=o->view.flags_810;v->flags_814=o->view.flags_814;
    v->affiliation=o->damage.effects.affiliation;v->action=o->view.action_520;v->combat_action=s->action;
    v->primary=o->view.weapons[0];v->secondary=o->view.weapons[1];v->target_uid=v->target_handle=UINT32_MAX;
    v->handle=o->registration.handle;
    memcpy(v->position,o->position,12);memcpy(v->basis,o->basis,36);memcpy(v->mount,s->mount,36);
    memcpy(v->angles,s->angles,12);v->angular=s->angular;
    if(!o->dead) {
        status=scene_turret_generated_checkpoint_target(o->target,0,&row->target_role,&v->target_uid);if(status)return status;
        v->target_handle=o->target;v->remaining=o->fire_due>frame?o->fire_due-frame:0;v->burst=s->burst;
    }
    status=scene_turret_generated_checkpoint_live_base(NULL,b->key.base_uid,&row->base);if(status)return status;
    return scene_turret_generated_checkpoint_valid(row,frame);
}
static void scene_turret_generated_checkpoint_encode(unsigned char *p,const scene_turret_generated_saved *r)
{
    scene_history_put(p,r->key.base_uid);scene_history_put(p+4,r->key.role);
    scene_turret_checkpoint_encode_row(p+8,&r->value);
    scene_history_put_float(p+192,r->base.health);scene_history_put(p+196,r->base.flags_7d0);
    scene_history_put(p+200,r->base.flags_810);scene_history_put(p+204,r->death_dispatched);
    scene_history_put(p+208,r->target_role);scene_history_put(p+212,r->head_flags_7d0);
}
static int scene_turret_generated_checkpoint_capture(unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,uint32_t frame,uint32_t budget,uint32_t *written)
{
    scene_turret_generated_saved *rows;uint32_t i,n=scene_turret_generated_count;int status;
    uint64_t required=16+(uint64_t)inner_bytes+(uint64_t)n*216,allocated=(uint64_t)n*sizeof(*rows);
    if(!p || !written || inner_bytes>capacity || frame==UINT32_MAX)return RF_RANGE;
    if(!n){*written=inner_bytes;return RF_OK;}
    if(n>SCENE_TURRET_OWNER_LIMIT || required>capacity || required>RF_CHECKPOINT_FILE_MAX || allocated>budget)return RF_RANGE;
    if(inner_bytes>=8 && !memcmp(p,"RFTU",4) && scene_history_word(p+4)!=1)return RF_FORMAT;
    rows=calloc(n,sizeof(*rows));if(!rows)return RF_IO;
    /* Capture once before modifying caller bytes; no partially rewritten blob. */
    for(i=0;i<n;i++){status=scene_turret_generated_checkpoint_capture_row(i,frame,rows+i);if(status){free(rows);return status;}}
    memmove(p+16,p,inner_bytes);memcpy(p,"RFTU",4);scene_history_put(p+4,2);
    scene_history_put(p+8,inner_bytes);scene_history_put(p+12,n);
    for(i=0;i<n;i++)scene_turret_generated_checkpoint_encode(p+16+inner_bytes+i*216,rows+i);
    free(rows);*written=(uint32_t)required;return RF_OK;
}
static int scene_turret_generated_checkpoint_decode(const unsigned char *p,scene_turret_generated_saved *r)
{
    scene_turret_saved *v=&r->value;const unsigned char *q=p+8;uint32_t i;
    r->head_flags_7d0=scene_history_word(p+212);
    r->key.base_uid=scene_history_word(p);r->key.role=scene_history_word(p+4);
    v->uid=scene_history_word(q);v->class_index=scene_history_word(q+4);
    v->health=scene_history_float(q+8);v->armor=scene_history_float(q+12);
    v->dead=scene_history_word(q+16);v->flags=scene_history_word(q+20);
    v->flags_810=scene_history_word(q+24);v->flags_814=scene_history_word(q+28);
    v->affiliation=scene_history_word(q+32);v->action=(int32_t)scene_history_word(q+36);
    v->target_uid=scene_history_word(q+40);v->remaining=scene_history_word(q+44);
    v->burst=scene_history_word(q+48);v->primary=(int32_t)scene_history_word(q+52);
    v->secondary=(int32_t)scene_history_word(q+56);v->combat_action=(int32_t)scene_history_word(q+60);
    for(i=0;i<3;i++){
        v->position[i]=scene_history_float(q+64+i*4);v->angles[i]=scene_history_float(q+112+i*4);
        v->angular.velocity[i]=scene_history_float(q+124+i*4);v->angular.force[i]=scene_history_float(q+136+i*4);}
    for(i=0;i<9;i++){v->basis[i]=scene_history_float(q+76+i*4);v->mount[i]=scene_history_float(q+148+i*4);}
    r->base.health=scene_history_float(p+192);r->base.flags_7d0=scene_history_word(p+196);
    r->base.flags_810=scene_history_word(p+200);r->death_dispatched=scene_history_word(p+204);
    r->base.retired=0; /* Not a serialized field; pending NPC admission proves it. */
    r->target_role=scene_history_word(p+208);return RF_OK;
}
/* Parse/resolve only. No live owner or pose changes. Dead generated owners must
 * already have a parent-reserved restore owner; live-only factory cannot create
 * them after dead NPC checkpoint publication. */
static int scene_turret_generated_checkpoint_prepare(const unsigned char *p,uint32_t bytes,
    uint32_t frame,uint32_t budget,scene_turret_generated_checkpoint_stage **out,
    const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_turret_generated_checkpoint_stage *stage;const unsigned char *body;
    uint32_t n,size,i,j,handle;uint64_t allocated;int status;
    if(!out || *out || !inner || !inner_bytes || frame==UINT32_MAX)return RF_RANGE;
    status=scene_turret_generated_checkpoint_inner(p,bytes,&body,&size,&n);if(status)return status;
    if(!n){
        /* Transport peeling remains boot-safe, but a composed restore may
         * not retain freshly created heads over a legacy saved world. */
        if(scene_turret_generated_count)return RF_NOT_FOUND;
        *inner=body;*inner_bytes=size;return RF_OK;
    }
    allocated=sizeof(*stage)+(uint64_t)n*sizeof(*stage->rows);
    if(n!=scene_turret_generated_count || allocated>budget || allocated>UINT32_MAX)return RF_RANGE;
    stage=calloc(1,(size_t)allocated);if(!stage)return RF_IO;
    stage->count=n;stage->frame=frame;stage->allocated_bytes=(uint32_t)allocated;
    for(i=0;i<n;i++) {
        scene_turret_generated_saved *r=stage->rows+i;scene_turret_owner *o;scene_turret_generated_binding *b;
        status=scene_turret_generated_checkpoint_decode(body+size+i*216,r);if(status)goto fail;
        for(j=0;j<i;j++)if(stage->rows[j].key.base_uid==r->key.base_uid){status=RF_FORMAT;goto fail;}
        status=scene_turret_generated_resolve(r->key,&handle);if(status)goto fail;
        o=scene_turret_lookup(handle);b=scene_turret_generated_find(handle);
        if(!o || !b){status=RF_NOT_FOUND;goto fail;}
        r->index=(uint32_t)(o-scene_turrets);r->value.handle=handle;r->base_handle=b->base_handle;
        status=scene_turret_generated_checkpoint_valid(r,frame);if(status)goto fail;
        status=scene_turret_generated_checkpoint_target(r->value.target_uid,1,&r->target_role,&r->value.target_handle);if(status)goto fail;
        if(r->value.target_handle==handle){status=RF_FORMAT;goto fail;}
    }
    *out=stage;*inner=body;*inner_bytes=size;return RF_OK;
fail:
    scene_turret_generated_checkpoint_close(&stage);return status;
}
/* Admit alongside pending NPC + authored turret stages, before any assigns.
 * This compares saved base health/death with the pending NPC source, not with
 * whatever damage/AI happened in the running scene after the save. */
static int scene_turret_generated_checkpoint_admit(const scene_turret_generated_checkpoint_stage *stage,
    scene_turret_generated_base_reader read_base,scene_turret_generated_target_reader read_target,void *context)
{
    uint32_t i,handle,target,role,alive;int status;
    if(!stage)return scene_turret_generated_count?RF_NOT_FOUND:RF_OK;
    if(!read_base || !read_target || stage->count!=scene_turret_generated_count)return RF_RANGE;
    for(i=0;i<stage->count;i++) {
        const scene_turret_generated_saved *r=stage->rows+i;scene_turret_generated_binding *b;
        scene_turret_generated_base_saved base={0};
        status=scene_turret_generated_checkpoint_valid(r,stage->frame);if(status)return status;
        status=scene_turret_generated_resolve(r->key,&handle);if(status)return status;
        b=scene_turret_generated_find(handle);
        if(!b || handle!=r->value.handle || b->base_handle!=r->base_handle || !scene_turret_generated_base(b))return RF_NOT_FOUND;
        role=r->target_role;status=scene_turret_generated_checkpoint_target(r->value.target_uid,1,&role,&target);if(status)return status;
        if(target!=r->value.target_handle || target==handle)return RF_NOT_FOUND;
        if(role || r->value.target_uid!=UINT32_MAX){
            alive=UINT32_MAX;
            status=read_target(context,role,r->value.target_uid,&alive);if(status)return status;
            if(alive!=1)return RF_FORMAT;
        }
        /* Sentinel requires callers to supply the pending retirement fact,
         * rather than accidentally authorizing a base which NPC assign frees. */
        base.retired=UINT32_MAX;
        status=read_base(context,r->key.base_uid,&base);if(status)return status;
        if(base.retired)return RF_NOT_FOUND;
        if(base.health!=r->base.health || (base.flags_810&1u)!=(r->base.flags_810&1u))return RF_FORMAT;
    }
    return RF_OK;
}
/* No allocation/callback/failure during assignment. Parent MUST admit every
 * stage first and prohibit registry changes between admission and commit.
 * Assign NPC/base pose first; then heads; then republish live tag positions. */
uint32_t rf_scene_turret_generated_restore_probe[32]; /* Last committed generated row, captured before simulation. */
static void scene_turret_generated_checkpoint_assign(const scene_turret_generated_checkpoint_stage *stage)
{
    uint32_t i;if(!stage)return;
    for(i=0;i<stage->count;i++) {
        const scene_turret_generated_saved *r=stage->rows+i;const scene_turret_saved *v=&r->value;
        scene_turret_owner *o=scene_turrets+r->index;scene_turret_combat_state *s=scene_turret_combat_states+r->index;
        scene_turret_generated_binding *b=scene_turret_generated_find(v->handle);
        campaign_npc_body *base=campaign_npc_bodies+b->base_slot;
        o->dead=v->dead;o->model=v->dead?o->dead_model:o->live_model;
        o->damage.effects.health=v->health;o->damage.effects.armor=v->armor;
        o->view.flags_7c=v->flags;o->view.flags_810=o->damage.effects.flags_810=v->flags_810;
        o->view.flags_814=o->damage.effects.flags_814=v->flags_814;
        o->view.flags_7d0=r->head_flags_7d0;o->view.linked_handle=v->dead?-1:(int32_t)b->base_handle;
        o->damage.effects.affiliation=v->affiliation;o->damage.effects.burn=0;
        o->damage.effects.voice=o->damage.responsible_handle=o->damage.burn_source=UINT32_MAX;
        o->damage.last_damage_time=0;o->view.action_520=v->action;
        o->view.weapons[0]=v->primary;o->view.weapons[1]=v->secondary;
        memcpy(o->position,v->position,12);memcpy(o->basis,v->basis,36);
        o->target=v->target_handle;o->fire_due=v->remaining?stage->frame+v->remaining:0;
        s->last_frame=UINT32_MAX;s->action=v->combat_action;s->burst=v->burst;
        memcpy(s->angles,v->angles,12);memcpy(s->mount,v->mount,36);s->angular=v->angular;
        b->death_dispatched=r->death_dispatched;
        /* NPC codec does not retain generic AI flags; this companion owns the
         * generated base readiness/AI snapshot and its firing mirror. */
        base->view.flags_7d0=r->base.flags_7d0;base->firing.flags_7d0=r->base.flags_7d0;
        {uint32_t *p=rf_scene_turret_generated_restore_probe;
         ++p[0];p[1]=r->key.base_uid;p[2]=r->key.role;p[3]=v->handle;p[4]=b->base_handle;
         p[5]=o->dead;memcpy(p+6,&o->damage.effects.health,4);memcpy(p+7,&o->damage.effects.armor,4);
         p[8]=r->target_role;p[9]=v->target_uid;p[10]=o->target;p[11]=v->remaining;p[12]=o->fire_due;
         p[13]=stage->frame;p[14]=s->burst;p[15]=(uint32_t)s->action;
         memcpy(p+16,o->basis,36);memcpy(p+25,s->angles,12);memcpy(p+28,o->position,12);p[31]=b->death_dispatched;}

    }
}
