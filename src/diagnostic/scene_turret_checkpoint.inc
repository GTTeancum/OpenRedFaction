#ifndef RF_SCENE_TURRET_CHECKPOINT_INCLUDED
#define RF_SCENE_TURRET_CHECKPOINT_INCLUDED
/* Optional RFTU1 wrapper within RF_WORLD_VEHICLE. Its inner payload remains
 * an unchanged RFVA/vehicle/RFVR record, or empty for a turret-only level.
 * Include after turret combat and scene_history scalar codecs. RFWC supplies
 * source identity/checksum. Legacy unwrapped saves keep authored turret state.
 * Rows address the stable authored prefix; RFTU2 owns generated suffix rows. */
enum {SCENE_TURRET_CHECKPOINT_HEADER=16,SCENE_TURRET_CHECKPOINT_ROW=184};
typedef struct scene_turret_saved {
    uint32_t uid,class_index,dead,flags,flags_810,flags_814,affiliation;
    float health,armor,position[3],basis[9],angles[3],mount[9];
    rf_angular_velocity_state angular;
    int32_t action,combat_action,primary,secondary;
    uint32_t target_uid,remaining,burst,handle,target_handle;
} scene_turret_saved;
typedef struct scene_turret_checkpoint_stage {
    uint32_t count,inner_bytes,allocated_bytes,frame;
    scene_turret_saved rows[];
} scene_turret_checkpoint_stage;
uint32_t rf_scene_turret_checkpoint[6]; /* captures,restores,rows,dead,targeted,last error */

static void scene_turret_checkpoint_close(scene_turret_checkpoint_stage **stage)
{if(stage&&*stage){free(*stage);*stage=NULL;}}

/* This transport-only peel also serves boot readers that do not yet own live
 * turret objects. An absent wrapper is legacy, not an error. */
static int scene_turret_checkpoint_inner(const unsigned char *p,uint32_t bytes,
    const unsigned char **inner,uint32_t *inner_bytes,uint32_t *count)
{
    uint32_t n,size;uint64_t required;
    if(!inner||!inner_bytes||!count||(bytes&&!p))return RF_RANGE;
    if(bytes<4||memcmp(p,"RFTU",4)){*inner=p;*inner_bytes=bytes;*count=0;return RF_OK;}
    if(bytes<SCENE_TURRET_CHECKPOINT_HEADER||scene_history_word(p+4)!=1)return RF_FORMAT;
    size=scene_history_word(p+8);n=scene_history_word(p+12);
    required=SCENE_TURRET_CHECKPOINT_HEADER+(uint64_t)size+(uint64_t)n*SCENE_TURRET_CHECKPOINT_ROW;
    if(!n||n>SCENE_TURRET_OWNER_LIMIT||required!=bytes||required>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
    *inner=p+SCENE_TURRET_CHECKPOINT_HEADER;*inner_bytes=size;*count=n;return RF_OK;
}

/* UINT_MAX means none; UID0 is the player, matching NPC attack checkpoints.
 * Ordinary targets are resolved by authored UID and fresh registry identity. */
static int scene_turret_checkpoint_target(uint32_t value,uint32_t decode,uint32_t *out)
{
    uint32_t i;
    if(!out)return RF_RANGE;
    if(value==UINT32_MAX){*out=UINT32_MAX;return RF_OK;}
    if((decode&&value==0)||(!decode&&value==campaign_player_object.handle)) {
        if(!campaign_player_object.view||rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view)return RF_NOT_FOUND;
        *out=decode?campaign_player_object.handle:0;return RF_OK;
    }
    if(campaign_authored_vehicle_uid && value==(decode?(uint32_t)campaign_authored_vehicle_uid:campaign_authored_vehicle_handle)) {
        if(!rf_object_registry_lookup(&campaign_registry,campaign_authored_vehicle_handle))return RF_NOT_FOUND;
        *out=decode?campaign_authored_vehicle_handle:(uint32_t)campaign_authored_vehicle_uid;return RF_OK;
    }
    for(i=0;i<campaign_npc_body_count;i++) {
        const campaign_npc_body *o=campaign_npc_bodies+i;
        uint32_t uid=(uint32_t)campaign_seeds.records.items[i].record.uid;
        if(o->registration.view&&value==(decode?uid:o->registration.handle)&&
           rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)==&o->view) {
            *out=decode?o->registration.handle:uid;return RF_OK;
        }
    }
    for(i=0;i<campaign_passive_vehicle_count;i++) {
        const scene_passive_vehicle *o=campaign_passive_vehicles+i;
        if(value==(decode?o->uid:o->handle)&&rf_object_registry_lookup(&campaign_registry,o->handle)==o) {
            *out=decode?o->handle:o->uid;return RF_OK;
        }
    }
    for(i=0;i<scene_turret_count;i++) {
        const scene_turret_owner *o=scene_turrets+i;
        if(value==(decode?o->uid:o->registration.handle)&&scene_turret_lookup(o->registration.handle)==o) {
            /* A generated head has no authored UID. Never encode its valid
             * handle as UINT_MAX (none), even with no pending fire cadence. */
            if(o->uid==UINT32_MAX||i>=scene_turret_authored_count)return RF_NOT_FOUND;
            *out=decode?o->registration.handle:o->uid;return RF_OK;
        }
    }
    return RF_NOT_FOUND;
}

static int scene_turret_checkpoint_row_valid(const scene_turret_saved *v,uint32_t index)
{
    const scene_turret_owner *o;const scene_turret_combat_state *s;const rf_entity_seed_class *c;
    float local[9],basis[9];uint32_t i;int status;
    if(!v||index>=scene_turret_authored_count||scene_turret_authored_count>scene_turret_count||
       scene_turret_count>SCENE_TURRET_OWNER_LIMIT||!scene_turret_combat_ready)return RF_RANGE;
    o=scene_turrets+index;s=scene_turret_combat_states+index;
    if(v->uid!=o->uid||v->class_index!=o->class_index||!v->uid||v->uid==UINT32_MAX||
       scene_turret_lookup(o->registration.handle)!=o||s->handle!=o->registration.handle||
       !isfinite(v->health)||!isfinite(v->armor)||fabsf(v->health)>1e7f||v->armor<0||v->armor>1e7f||
       v->dead>1||v->dead!=(v->health<=0)||!!(v->flags_810&1u)!=v->dead||
       v->action< -1||v->action>13||v->combat_action< -1||v->combat_action>13||
       v->primary< -1||v->primary>=64||v->secondary< -1||v->secondary>=64||
       v->remaining>36000||v->burst>scene_turret_primary.burst_count||
       (v->target_uid==UINT32_MAX&&(v->remaining||v->burst))||
       (v->dead&&(v->target_uid!=UINT32_MAX||v->remaining||v->burst)))return RF_FORMAT;
    c=campaign_seeds.classes+o->class_index;
    for(i=0;i<3;i++)if(!isfinite(v->position[i])||!isfinite(v->angles[i])||
        v->angles[i]<c->eye_limits.minimum[i]-.0001f||v->angles[i]>c->eye_limits.maximum[i]+.0001f||
        !isfinite(v->angular.velocity[i])||!isfinite(v->angular.force[i]))return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(v->basis[i])||!isfinite(v->mount[i])||v->mount[i]!=s->mount[i])return RF_FORMAT;
    status=rf_look_orientation(v->angles,local);if(status)return status;
    for(i=0;i<3;i++)scene_turret_world_vector(v->mount,local+i*3,basis+i*3);
    for(i=0;i<9;i++)if(fabsf(v->basis[i]-basis[i])>.0001f)return RF_FORMAT;
    return RF_OK;
}

static int scene_turret_checkpoint_capture_row(uint32_t index,uint32_t frame,scene_turret_saved *v)
{
    const scene_turret_owner *o;const scene_turret_combat_state *s;
    int status;
    if(!v||!scene_turrets||index>=scene_turret_authored_count||scene_turret_authored_count>scene_turret_count||
       scene_turret_count>SCENE_TURRET_OWNER_LIMIT)return RF_RANGE;
    o=scene_turrets+index;s=scene_turret_combat_states+index;
    if(o->damage.effects.burn||o->view.linked_handle!=-1||(o->view.occupant_count&&!scene_npc_seat_checkpoint_host(o->registration.handle)))return RF_NOT_FOUND;
    if(o->model!=(o->dead?o->dead_model:o->live_model))return RF_FORMAT;
    memset(v,0,sizeof(*v));v->uid=o->uid;v->class_index=o->class_index;v->dead=o->dead;
    v->health=o->damage.effects.health;v->armor=o->damage.effects.armor;
    v->flags=o->view.flags_7c;v->flags_810=o->view.flags_810;v->flags_814=o->view.flags_814;
    v->affiliation=o->damage.effects.affiliation;v->action=o->view.action_520;v->combat_action=s->action;
    v->primary=o->view.weapons[0];v->secondary=o->view.weapons[1];v->target_uid=UINT32_MAX;
    memcpy(v->position,o->position,12);memcpy(v->basis,o->basis,36);memcpy(v->mount,s->mount,36);
    memcpy(v->angles,s->angles,12);v->angular=s->angular;
    if(!o->dead) {
        status=scene_turret_checkpoint_target(o->target,0,&v->target_uid);if(status)return status;
        v->remaining=o->fire_due>frame?o->fire_due-frame:0;v->burst=s->burst;
    }
    return scene_turret_checkpoint_row_valid(v,index);
}

static void scene_turret_checkpoint_encode_row(unsigned char *p,const scene_turret_saved *v)
{
    uint32_t i;
    scene_history_put(p,v->uid);scene_history_put(p+4,v->class_index);
    scene_history_put_float(p+8,v->health);scene_history_put_float(p+12,v->armor);
    scene_history_put(p+16,v->dead);scene_history_put(p+20,v->flags);
    scene_history_put(p+24,v->flags_810);scene_history_put(p+28,v->flags_814);
    scene_history_put(p+32,v->affiliation);scene_history_put(p+36,(uint32_t)v->action);
    scene_history_put(p+40,v->target_uid);scene_history_put(p+44,v->remaining);
    scene_history_put(p+48,v->burst);scene_history_put(p+52,(uint32_t)v->primary);
    scene_history_put(p+56,(uint32_t)v->secondary);scene_history_put(p+60,(uint32_t)v->combat_action);
    for(i=0;i<3;i++){scene_history_put_float(p+64+i*4,v->position[i]);scene_history_put_float(p+112+i*4,v->angles[i]);
        scene_history_put_float(p+124+i*4,v->angular.velocity[i]);scene_history_put_float(p+136+i*4,v->angular.force[i]);}
    for(i=0;i<9;i++){scene_history_put_float(p+76+i*4,v->basis[i]);scene_history_put_float(p+148+i*4,v->mount[i]);}
}

static int scene_turret_checkpoint_capture(unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,uint32_t frame,uint32_t *written)
{
    scene_turret_saved row;uint32_t i;int status;
    status=scene_turret_generated_activation_checkpoint_admit();if(status)return status;
    uint64_t required=SCENE_TURRET_CHECKPOINT_HEADER+(uint64_t)inner_bytes+(uint64_t)scene_turret_authored_count*SCENE_TURRET_CHECKPOINT_ROW;
    if(!p||!written||inner_bytes>capacity||frame==UINT32_MAX)return RF_RANGE;
    if(scene_turret_authored_count>scene_turret_count)return RF_RANGE;
    if(!scene_turret_authored_count){*written=inner_bytes;return RF_OK;}
    if(scene_turret_count>SCENE_TURRET_OWNER_LIMIT||!scene_turrets||!scene_turret_combat_ready||required>capacity||required>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    for(i=0;i<scene_turret_authored_count;i++){status=scene_turret_checkpoint_capture_row(i,frame,&row);if(status)return status;}
    memmove(p+SCENE_TURRET_CHECKPOINT_HEADER,p,inner_bytes);memcpy(p,"RFTU",4);
    scene_history_put(p+4,1);scene_history_put(p+8,inner_bytes);scene_history_put(p+12,scene_turret_authored_count);
    for(i=0;i<scene_turret_authored_count;i++) {
        status=scene_turret_checkpoint_capture_row(i,frame,&row);if(status)return status;
        scene_turret_checkpoint_encode_row(p+SCENE_TURRET_CHECKPOINT_HEADER+inner_bytes+i*SCENE_TURRET_CHECKPOINT_ROW,&row);
    }
    ++rf_scene_turret_checkpoint[0];*written=(uint32_t)required;return RF_OK;
}

static int scene_turret_checkpoint_prepare(const unsigned char *p,uint32_t bytes,uint32_t frame,
    uint32_t budget,scene_turret_checkpoint_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_turret_checkpoint_stage *stage;const unsigned char *body;uint32_t count,size,i,j;uint64_t allocated;int status;
    if(!out||*out||!inner||!inner_bytes||frame==UINT32_MAX)return RF_RANGE;
    status=scene_turret_checkpoint_inner(p,bytes,&body,&size,&count);if(status)return status;
    if(!count){*inner=body;*inner_bytes=size;return RF_OK;}
    allocated=sizeof(*stage)+(uint64_t)count*sizeof(*stage->rows);
    if(count!=scene_turret_authored_count||scene_turret_authored_count>scene_turret_count||
       scene_turret_count>SCENE_TURRET_OWNER_LIMIT||allocated>budget||allocated>UINT32_MAX)return RF_FORMAT;
    stage=calloc(1,(size_t)allocated);if(!stage)return RF_IO;
    stage->count=count;stage->inner_bytes=size;stage->allocated_bytes=(uint32_t)allocated;stage->frame=frame;
    for(i=0;i<count;i++) {
        const unsigned char *q=body+size+i*SCENE_TURRET_CHECKPOINT_ROW;scene_turret_saved *v=stage->rows+i;
        v->uid=scene_history_word(q);v->class_index=scene_history_word(q+4);
        v->health=scene_history_float(q+8);v->armor=scene_history_float(q+12);
        v->dead=scene_history_word(q+16);v->flags=scene_history_word(q+20);
        v->flags_810=scene_history_word(q+24);v->flags_814=scene_history_word(q+28);
        v->affiliation=scene_history_word(q+32);v->action=(int32_t)scene_history_word(q+36);
        v->target_uid=scene_history_word(q+40);v->remaining=scene_history_word(q+44);
        v->burst=scene_history_word(q+48);v->primary=(int32_t)scene_history_word(q+52);
        v->secondary=(int32_t)scene_history_word(q+56);v->combat_action=(int32_t)scene_history_word(q+60);
        for(j=0;j<3;j++){v->position[j]=scene_history_float(q+64+j*4);v->angles[j]=scene_history_float(q+112+j*4);
            v->angular.velocity[j]=scene_history_float(q+124+j*4);v->angular.force[j]=scene_history_float(q+136+j*4);}
        for(j=0;j<9;j++){v->basis[j]=scene_history_float(q+76+j*4);v->mount[j]=scene_history_float(q+148+j*4);}
        status=scene_turret_checkpoint_row_valid(v,i);if(status)goto fail;
        if(v->remaining>UINT32_MAX-frame){status=RF_RANGE;goto fail;}
        v->handle=scene_turrets[i].registration.handle;
        status=scene_turret_checkpoint_target(v->target_uid,1,&v->target_handle);if(status)goto fail;
        if(v->target_handle==v->handle){status=RF_FORMAT;goto fail;}
    }
    *out=stage;*inner=body;*inner_bytes=size;return RF_OK;
fail:
    rf_scene_turret_checkpoint[5]=(uint32_t)status;scene_turret_checkpoint_close(&stage);return status;
}

static int scene_turret_checkpoint_validate(const scene_turret_checkpoint_stage *stage)
{
    uint32_t i,target;int status;if(!stage)return RF_OK;
    if(stage->count!=scene_turret_authored_count||scene_turret_authored_count>scene_turret_count)return RF_FORMAT;
    for(i=0;i<stage->count;i++) {
        const scene_turret_saved *v=stage->rows+i;
        status=scene_turret_checkpoint_row_valid(v,i);if(status)return status;
        if(v->handle!=scene_turrets[i].registration.handle)return RF_NOT_FOUND;
        status=scene_turret_checkpoint_target(v->target_uid,1,&target);if(status)return status;
        if(target!=v->target_handle)return RF_NOT_FOUND;
    }
    return RF_OK;
}

uint32_t rf_scene_turret_restore_probe[8];
static void scene_turret_checkpoint_assign(const scene_turret_checkpoint_stage *stage)
{
    uint32_t i;if(!stage)return;
    for(i=0;i<stage->count;i++) {
        const scene_turret_saved *v=stage->rows+i;scene_turret_owner *o=scene_turrets+i;
        scene_turret_combat_state *s=scene_turret_combat_states+i;
        o->dead=v->dead;o->model=v->dead?o->dead_model:o->live_model;
        o->damage.effects.health=v->health;o->damage.effects.armor=v->armor;
        o->view.flags_7c=v->flags;o->view.flags_810=o->damage.effects.flags_810=v->flags_810;
        o->view.flags_814=o->damage.effects.flags_814=v->flags_814;
        o->damage.effects.affiliation=v->affiliation;o->damage.effects.burn=0;
        o->damage.effects.voice=o->damage.responsible_handle=o->damage.burn_source=UINT32_MAX;
        o->damage.last_damage_time=0;o->view.action_520=v->action;
        o->view.weapons[0]=v->primary;o->view.weapons[1]=v->secondary;
        memcpy(o->position,v->position,12);memcpy(o->basis,v->basis,36);
        o->target=v->target_handle;o->fire_due=v->remaining?stage->frame+v->remaining:0;
        s->last_frame=UINT32_MAX;s->action=v->combat_action;s->burst=v->burst;
        rf_scene_turret_restore_probe[0]=v->uid;rf_scene_turret_restore_probe[1]=v->target_uid;
        rf_scene_turret_restore_probe[2]=o->target;rf_scene_turret_restore_probe[3]=v->remaining;
        rf_scene_turret_restore_probe[4]=o->fire_due;rf_scene_turret_restore_probe[5]=stage->frame;
        rf_scene_turret_restore_probe[6]=s->burst;rf_scene_turret_restore_probe[7]=(uint32_t)s->action;
        memcpy(s->angles,v->angles,12);s->angular=v->angular;
        rf_scene_turret_checkpoint[3]+=v->dead;rf_scene_turret_checkpoint[4]+=v->target_uid!=UINT32_MAX;
    }
    ++rf_scene_turret_checkpoint[1];rf_scene_turret_checkpoint[2]=stage->count;
}
#endif
