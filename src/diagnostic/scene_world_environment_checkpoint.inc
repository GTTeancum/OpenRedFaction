#ifndef RF_SCENE_WORLD_ENVIRONMENT_CHECKPOINT_INCLUDED
#define RF_SCENE_WORLD_ENVIRONMENT_CHECKPOINT_INCLUDED
/* RFEN1: identity-bound gravity and force-volume mutable state. Geometry remains
 * authored, matched by UID and explicit immutable-field fingerprint. No handles.
 * Queued level handoffs are not admitted; presentation state is separate. */
enum {SCENE_ENV_HEADER=80,SCENE_ENV_ROW=16,SCENE_ENV_FORCE_MAX=65536/sizeof(rf_physics_force_region)};
typedef struct scene_environment_entry {
    uint32_t slot,active;float strength;rf_physics_force_region before;
} scene_environment_entry;
typedef struct scene_world_environment_stage {
    rf_physics_gravity before,next;rf_physics_force_region *owner;uint32_t count,bytes;
    scene_environment_entry rows[];
} scene_world_environment_stage;
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
static inline int scene_world_environment_encode(const unsigned char identity[32],void *out,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=out;uint32_t i,j,bytes;int status;
    if(!identity||!out||!written||campaign_forces.count>SCENE_ENV_FORCE_MAX||
       (campaign_forces.count&&!campaign_forces.items))return RF_RANGE;
    if(rf_scene_level_transition.pending)return RF_NOT_FOUND;
    status=scene_environment_gravity_valid(&scene_gravity);if(status)return status;
    bytes=SCENE_ENV_HEADER+campaign_forces.count*SCENE_ENV_ROW;if(bytes>capacity)return RF_RANGE;
    for(i=0;i<campaign_forces.count;i++){
        if(!isfinite(campaign_forces.items[i].strength))return RF_FORMAT;
        for(j=0;j<i;j++)if(campaign_forces.items[j].uid==campaign_forces.items[i].uid)return RF_FORMAT;
    }
    memset(p,0,bytes);memcpy(p,"RFEN",4);scene_history_put(p+4,1);scene_history_put(p+8,bytes);
    scene_history_put(p+16,campaign_forces.count);memcpy(p+24,identity,32);
    scene_history_put_float(p+56,scene_gravity.acceleration);
    for(i=0;i<3;i++)scene_history_put_float(p+60+i*4,scene_gravity.vector[i]);
    for(i=0;i<campaign_forces.count;i++){
        const rf_physics_force_region *r=campaign_forces.items+i;unsigned char *row=p+80+i*16;
        scene_history_put(row,r->uid);scene_history_put(row+4,r->active);scene_history_put_float(row+8,r->strength);scene_history_put(row+12,scene_environment_geometry(r));
    }
    scene_history_put(p+12,scene_history_hash(p,bytes));*written=bytes;return RF_OK;
}
static inline void scene_world_environment_close(scene_world_environment_stage **s)
{if(s){free(*s);*s=NULL;}}
static inline int scene_world_environment_prepare(const unsigned char identity[32],const void *data,uint32_t bytes,
    uint32_t budget,scene_world_environment_stage **out)
{
    const unsigned char *p=data;scene_world_environment_stage *s;uint32_t count,i,j,k;uint64_t cost;int status=RF_OK;
    if(!identity||!data||!out||*out)return RF_RANGE;
    if(rf_scene_level_transition.pending)return RF_NOT_FOUND;
    if(bytes<80||memcmp(p,"RFEN",4)||scene_history_word(p+4)!=1||scene_history_word(p+8)!=bytes||
       scene_history_word(p+12)!=scene_history_hash(p,bytes)||memcmp(p+24,identity,32)||scene_history_word(p+20)||
       scene_history_word(p+72)||scene_history_word(p+76))return RF_FORMAT;
    count=scene_history_word(p+16);
    if(count>SCENE_ENV_FORCE_MAX||count!=campaign_forces.count||bytes!=80+count*16||(count&&!campaign_forces.items))return RF_FORMAT;
    cost=sizeof(*s)+(uint64_t)count*sizeof(*s->rows);if(cost>budget)return RF_RANGE;
    s=calloc(1,(size_t)cost);if(!s)return RF_IO;s->count=count;s->bytes=(uint32_t)cost;s->owner=campaign_forces.items;s->before=scene_gravity;
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
    *out=s;return RF_OK;
fail:free(s);return status;
}
static inline int scene_world_environment_validate(const scene_world_environment_stage *s)
{
    uint32_t i;if(!s||rf_scene_level_transition.pending||s->count!=campaign_forces.count||s->owner!=campaign_forces.items||
        memcmp(&s->before,&scene_gravity,sizeof(scene_gravity)))return RF_FORMAT;
    for(i=0;i<s->count;i++)if(memcmp(&s->rows[i].before,s->owner+s->rows[i].slot,sizeof(rf_physics_force_region)))return RF_FORMAT;
    return RF_OK;
}
static inline void scene_world_environment_assign(const scene_world_environment_stage *s)
{
    uint32_t i;scene_gravity=s->next;
    for(i=0;i<s->count;i++){s->owner[s->rows[i].slot].active=s->rows[i].active;s->owner[s->rows[i].slot].strength=s->rows[i].strength;}
}
#endif
