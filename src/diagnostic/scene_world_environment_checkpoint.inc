#ifndef RF_SCENE_WORLD_ENVIRONMENT_CHECKPOINT_INCLUDED
#define RF_SCENE_WORLD_ENVIRONMENT_CHECKPOINT_INCLUDED
/* RFEN3: identity-bound gravity, force-volume state, changed navigation radii,
 * blackout timer, player support mover UID and portable cutscene clock/camera. Navigation rows use authored UIDs and
 * raw radius bits, never indices. RFEN1/2 remain readable. No handles are serialized.
 * Queued level handoffs are not admitted; other presentation state is separate. */
enum {SCENE_ENV_HEADER=80,SCENE_ENV_ROW=16,SCENE_ENV_NAV_ROW=8,
    SCENE_ENV_FORCE_MAX=65536/sizeof(rf_physics_force_region),SCENE_ENV_NAV_MAX=65536};
typedef struct scene_environment_entry {
    uint32_t slot,active;float strength;rf_physics_force_region before;
} scene_environment_entry;
typedef struct scene_environment_nav_entry {
    uint32_t slot,uid,before,after,baseline;
} scene_environment_nav_entry;
typedef struct scene_world_environment_stage {
    rf_physics_gravity before,next;rf_physics_force_region *owner;uint32_t count,bytes;
    rf_level_navigation_node *navigation;uint32_t navigation_count,nav_count;
    rf_cutscene_runtime cutscene_before,cutscene_next;
    int32_t blackout_before,blackout_next;uint32_t support_uid;
    scene_environment_entry rows[];
} scene_world_environment_stage;
static scene_environment_nav_entry *scene_environment_nav_rows(scene_world_environment_stage *s)
{return (scene_environment_nav_entry *)(s->rows+s->count);}
static uint32_t scene_environment_geometry(const rf_physics_force_region *r)
{
    unsigned char bytes[100];uint32_t i,at=12;
    scene_history_put(bytes,r->shape);scene_history_put(bytes+4,r->uid);scene_history_put(bytes+8,r->flags);
#define ENV_VECTOR(field,n) for(i=0;i<(n);i++){scene_history_put_float(bytes+at,r->field[i]);at+=4;}
    ENV_VECTOR(center,3);ENV_VECTOR(matrix,9);scene_history_put_float(bytes+at,r->radius_squared);at+=4;
    ENV_VECTOR(minimum,3);ENV_VECTOR(maximum,3);ENV_VECTOR(size,3);
#undef ENV_VECTOR
    /* All geometry bytes matter; unlike envelope hashes there is no checksum field. */
    {uint32_t hash=2166136261u;for(i=0;i<sizeof(bytes);i++)hash=(hash^bytes[i])*16777619u;return hash;}
}
static int scene_environment_gravity_valid(const rf_physics_gravity *g)
{return isfinite(g->acceleration)&&g->vector[0]==0&&g->vector[2]==0&&g->vector[1]==-g->acceleration?RF_OK:RF_FORMAT;}
static inline int scene_world_environment_encode(const unsigned char identity[32],int32_t now,void *out,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=out;uint32_t i,j,bytes,nav_count=0,support_uid=0;uint64_t required;int32_t blackout_remaining=0;int status;
    if(!identity||!out||!written||campaign_forces.count>SCENE_ENV_FORCE_MAX||
       (campaign_forces.count&&!campaign_forces.items)||campaign_navigation.count>SCENE_ENV_NAV_MAX||
       (campaign_navigation.count&&!campaign_navigation.nodes))return RF_RANGE;
    if(rf_scene_level_transition.pending)return RF_NOT_FOUND;
    status=scene_environment_gravity_valid(&scene_gravity);if(status)return status;
    for(i=0;i<campaign_forces.count;i++){
        if(!isfinite(campaign_forces.items[i].strength))return RF_FORMAT;
        for(j=0;j<i;j++)if(campaign_forces.items[j].uid==campaign_forces.items[i].uid)return RF_FORMAT;
    }
    for(i=0;i<campaign_navigation.count;i++){
        const rf_level_navigation_node *node=campaign_navigation.nodes+i;uint32_t bits;
        memcpy(&bits,&node->candidate.radius,4);
        if(bits==node->candidate.retained_018)continue;
        if(node->uid==UINT32_MAX||bits!=0)return RF_FORMAT;
        for(j=0;j<i;j++)if(campaign_navigation.nodes[j].uid==node->uid)return RF_FORMAT;
        ++nav_count;
    }
    required=SCENE_ENV_HEADER+(uint64_t)campaign_forces.count*SCENE_ENV_ROW+
        (uint64_t)nav_count*SCENE_ENV_NAV_ROW+RF_CUTSCENE_CHECKPOINT_BYTES;
    if(required>capacity||required>UINT32_MAX)return RF_RANGE;bytes=(uint32_t)required;
    memset(p,0,bytes);memcpy(p,"RFEN",4);scene_history_put(p+4,3);scene_history_put(p+8,bytes);
    scene_history_put(p+16,campaign_forces.count);scene_history_put(p+20,nav_count);memcpy(p+24,identity,32);
    if(campaign_blackout_deadline>=0){
        status=rf_timer_remaining(campaign_blackout_deadline,now,&blackout_remaining);if(status)return status;
        if(blackout_remaining>30000)return RF_FORMAT;
        if(blackout_remaining>0)scene_history_put(p+72,(uint32_t)blackout_remaining+1);
    }
    status=scene_checkpoint_player_support_uid(scene_actor_collision_owner,&support_uid);if(status)return status;
    scene_history_put(p+76,support_uid);
    scene_history_put_float(p+56,scene_gravity.acceleration);
    for(i=0;i<3;i++)scene_history_put_float(p+60+i*4,scene_gravity.vector[i]);
    for(i=0;i<campaign_forces.count;i++){
        const rf_physics_force_region *r=campaign_forces.items+i;unsigned char *row=p+80+i*16;
        scene_history_put(row,r->uid);scene_history_put(row+4,r->active);scene_history_put_float(row+8,r->strength);scene_history_put(row+12,scene_environment_geometry(r));
    }
    for(i=0,j=0;i<campaign_navigation.count;i++){
        const rf_level_navigation_node *node=campaign_navigation.nodes+i;unsigned char *row;uint32_t bits;
        memcpy(&bits,&node->candidate.radius,4);if(bits==node->candidate.retained_018)continue;
        row=p+80+campaign_forces.count*16+j++*SCENE_ENV_NAV_ROW;
        scene_history_put(row,node->uid);scene_history_put(row+4,bits);
    }
    status=rf_cutscene_checkpoint_encode(&campaign_cutscene_runtime,now,
        p+bytes-RF_CUTSCENE_CHECKPOINT_BYTES,RF_CUTSCENE_CHECKPOINT_BYTES);if(status)return status;
    scene_history_put(p+12,scene_history_hash(p,bytes));*written=bytes;return RF_OK;
}
static inline void scene_world_environment_close(scene_world_environment_stage **s)
{if(s){free(*s);*s=NULL;}}
static inline int scene_world_environment_prepare(const unsigned char identity[32],int32_t now,const void *data,uint32_t bytes,
    uint32_t budget,scene_world_environment_stage **out)
{
    const unsigned char *p=data;scene_world_environment_stage *s;uint32_t count,nav_count,version,i,j,k;uint64_t cost;int status=RF_OK;
    if(!identity||!data||!out||*out)return RF_RANGE;
    if(rf_scene_level_transition.pending)return RF_NOT_FOUND;
    if(bytes<80||memcmp(p,"RFEN",4)||scene_history_word(p+8)!=bytes||
       scene_history_word(p+12)!=scene_history_hash(p,bytes)||memcmp(p+24,identity,32))return RF_FORMAT;
    version=scene_history_word(p+4);if(version!=1&&version!=2&&version!=3)return RF_FORMAT;
    if((version<3&&(scene_history_word(p+72)||scene_history_word(p+76)))||scene_history_word(p+72)>30001||
       scene_history_word(p+76)==UINT32_MAX)return RF_FORMAT;
    count=scene_history_word(p+16);nav_count=scene_history_word(p+20);
    if((version==1&&nav_count)||count>SCENE_ENV_FORCE_MAX||count!=campaign_forces.count||
       nav_count>campaign_navigation.count||nav_count>SCENE_ENV_NAV_MAX||
       (uint64_t)bytes!=80u+(uint64_t)count*SCENE_ENV_ROW+(uint64_t)nav_count*SCENE_ENV_NAV_ROW+
           (version==3?RF_CUTSCENE_CHECKPOINT_BYTES:0)||
       (count&&!campaign_forces.items)||(nav_count&&!campaign_navigation.nodes))return RF_FORMAT;
    cost=sizeof(*s)+(uint64_t)count*sizeof(*s->rows)+(uint64_t)nav_count*sizeof(scene_environment_nav_entry);
    if(cost>budget||cost>UINT32_MAX)return RF_RANGE;
    s=calloc(1,(size_t)cost);if(!s)return RF_IO;s->count=count;s->bytes=(uint32_t)cost;s->owner=campaign_forces.items;
    s->navigation=campaign_navigation.nodes;s->navigation_count=campaign_navigation.count;s->nav_count=nav_count;s->before=scene_gravity;
    s->cutscene_before=campaign_cutscene_runtime;
    s->blackout_before=campaign_blackout_deadline;s->blackout_next=-1;
    s->support_uid=scene_history_word(p+76);
    if(scene_history_word(p+72)){
        status=rf_timer_set(&s->blackout_next,now,(int32_t)scene_history_word(p+72)-1);if(status)goto fail;
    }
    if(version==3){status=rf_cutscene_checkpoint_decode(&campaign_cutscene_resources,now,
        p+bytes-RF_CUTSCENE_CHECKPOINT_BYTES,RF_CUTSCENE_CHECKPOINT_BYTES,&s->cutscene_next);if(status)goto fail;}
    s->next.acceleration=scene_history_float(p+56);for(i=0;i<3;i++)s->next.vector[i]=scene_history_float(p+60+i*4);
    status=scene_environment_gravity_valid(&s->next);if(status)goto fail;
    for(i=0;i<count;i++){
        const unsigned char *r=p+80+i*16;scene_environment_entry *e=s->rows+i;uint32_t uid=scene_history_word(r),found=0;
        for(j=0;j<count;j++)if(campaign_forces.items[j].uid==uid){e->slot=j;found++;}
        if(found!=1){status=RF_FORMAT;goto fail;}
        for(k=0;k<i;k++)if(s->rows[k].slot==e->slot){status=RF_FORMAT;goto fail;}
        e->before=campaign_forces.items[e->slot];e->active=scene_history_word(r+4);e->strength=scene_history_float(r+8);
        if(!isfinite(e->strength)||scene_history_word(r+12)!=scene_environment_geometry(&e->before)){status=RF_FORMAT;goto fail;}
    }
    for(i=0;i<nav_count;i++){
        const unsigned char *r=p+80+count*SCENE_ENV_ROW+i*SCENE_ENV_NAV_ROW;
        scene_environment_nav_entry *e=scene_environment_nav_rows(s)+i;uint32_t found=0;
        e->uid=scene_history_word(r);e->after=scene_history_word(r+4);
        for(j=0;j<campaign_navigation.count;j++)if(campaign_navigation.nodes[j].uid==e->uid){e->slot=j;found=1;break;}
        if(!found){status=RF_FORMAT;goto fail;}
        for(k=0;k<i;k++)if(scene_environment_nav_rows(s)[k].slot==e->slot){status=RF_FORMAT;goto fail;}
        e->baseline=campaign_navigation.nodes[e->slot].candidate.retained_018;
        memcpy(&e->before,&campaign_navigation.nodes[e->slot].candidate.radius,4);
        if(e->after!=0&&e->after!=e->baseline){status=RF_FORMAT;goto fail;}
    }
    *out=s;return RF_OK;
fail:free(s);return status;
}
static inline int scene_world_environment_validate(const scene_world_environment_stage *s)
{
    uint32_t i;if(!s||rf_scene_level_transition.pending||s->count!=campaign_forces.count||s->owner!=campaign_forces.items||
        s->navigation!=campaign_navigation.nodes||s->navigation_count!=campaign_navigation.count||
        memcmp(&s->before,&scene_gravity,sizeof(scene_gravity))||
        memcmp(&s->cutscene_before,&campaign_cutscene_runtime,sizeof(campaign_cutscene_runtime))||
        s->blackout_before!=campaign_blackout_deadline)return RF_FORMAT;
    for(i=0;i<s->count;i++)if(memcmp(&s->rows[i].before,s->owner+s->rows[i].slot,sizeof(rf_physics_force_region)))return RF_FORMAT;
    for(i=0;i<s->nav_count;i++){
        const scene_environment_nav_entry *e=scene_environment_nav_rows((scene_world_environment_stage *)s)+i;
        uint32_t bits;
        if(e->slot>=s->navigation_count||s->navigation[e->slot].uid!=e->uid||
           s->navigation[e->slot].candidate.retained_018!=e->baseline)return RF_FORMAT;
        memcpy(&bits,&s->navigation[e->slot].candidate.radius,4);if(bits!=e->before)return RF_FORMAT;
    }
    return RF_OK;
}
static inline void scene_world_environment_assign(const scene_world_environment_stage *s)
{
    uint32_t i;scene_gravity=s->next;
    for(i=0;i<s->count;i++){s->owner[s->rows[i].slot].active=s->rows[i].active;s->owner[s->rows[i].slot].strength=s->rows[i].strength;}
    for(i=0;i<s->nav_count;i++){
        const scene_environment_nav_entry *e=scene_environment_nav_rows((scene_world_environment_stage *)s)+i;
        memcpy(&s->navigation[e->slot].candidate.radius,&e->after,4);
    }
    campaign_cutscene_runtime=s->cutscene_next;
    campaign_blackout_deadline=s->blackout_next;
    rf_scene_cutscene[3]=campaign_cutscene_runtime.active_uid;
    rf_scene_cutscene[4]=campaign_cutscene_runtime.point_index;
    rf_scene_cutscene[7]=campaign_cutscene_runtime.active;
    if(s->nav_count){
        const scene_environment_nav_entry *last=scene_environment_nav_rows((scene_world_environment_stage *)s)+s->nav_count-1;
        printf("WORLD_NAVPOINT_RESTORE count%u uid%u radius_bits%u\n",s->nav_count,last->uid,last->after);
    }
}
#endif
