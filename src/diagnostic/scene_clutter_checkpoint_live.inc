#include "rf/clutter_checkpoint.h"
typedef struct scene_clutter_restore_entry {rf_clutter_checkpoint_record row;uint32_t slot,handle;int32_t deadline;} scene_clutter_restore_entry;
typedef struct scene_clutter_restore_stage {uint32_t count;scene_clutter_restore_entry entries[];} scene_clutter_restore_stage;
static int scene_clutter_checkpoint_identity(const unsigned char source[32],unsigned char out[32])
{
    checkpoint_sha hash;uint32_t i,j;
    if(!source||!out||campaign_clutter_records.count>1024||
       (campaign_clutter_records.count&&(!campaign_clutter_bodies||!campaign_clutter_records.items||!campaign_clutter_damage_bindings||!campaign_clutter_damage_profiles)))return RF_RANGE;
    checkpoint_sha_init(&hash);checkpoint_sha_add(&hash,"RFPC static props v1",20);checkpoint_sha_add(&hash,source,32);
    checkpoint_sha_word(&hash,campaign_clutter_records.count);
    for(i=0;i<campaign_clutter_records.count;i++){
        const rf_level_clutter *r=campaign_clutter_records.items+i;rf_clutter_base_owner *owner=campaign_clutter_bodies[i];
        const scene_clutter_damage_profile *p;scene_clutter_damage_binding *binding=campaign_clutter_damage_bindings+i;
        const rf_clutter_class *cls;
        if(!owner||!scene_clutter_damage_identity(&campaign_registry,owner)||owner->uid!=r->uid||
           !binding->initialized||binding->handle!=owner->state.handle||binding->class_index!=owner->state.class_index||
           owner->state.class_index<0||(uint32_t)owner->state.class_index>=campaign_clutter_classes.count)return RF_FORMAT;
        cls=campaign_clutter_classes.items+owner->state.class_index;p=campaign_clutter_damage_profiles+owner->state.class_index;
        if(owner->state.definition!=cls||!p->ordinary||!r->class_name||!cls->name||!cls->model||
           memcmp(owner->state.position,r->position,12)||memcmp(owner->body.state.position,r->position,12)||
           memcmp(owner->matrix,r->matrix,36))return RF_RANGE;
        for(j=0;j<3;j++)if(owner->body.state.velocity[j]!=0||!isfinite(r->position[j]))return RF_RANGE;
        checkpoint_sha_word(&hash,r->uid);checkpoint_sha_word(&hash,(uint32_t)owner->state.class_index);
        checkpoint_sha_add(&hash,r->class_name,(uint32_t)strlen(r->class_name)+1);
        checkpoint_sha_add(&hash,cls->name,(uint32_t)strlen(cls->name)+1);checkpoint_sha_add(&hash,cls->model,(uint32_t)strlen(cls->model)+1);
        checkpoint_sha_word(&hash,cls->flags);checkpoint_sha_float(&hash,p->life);
        for(j=0;j<11;j++)checkpoint_sha_float(&hash,p->damage_factors[j]);
        checkpoint_sha_float(&hash,p->break_radius);checkpoint_sha_float(&hash,p->break_damage);
        for(j=0;j<3;j++){checkpoint_sha_float(&hash,r->position[j]);checkpoint_sha_float(&hash,p->break_offset[j]);}
        for(j=0;j<9;j++){if(!isfinite(r->matrix[j/3][j%3]))return RF_FORMAT;checkpoint_sha_float(&hash,r->matrix[j/3][j%3]);}
    }
    checkpoint_sha_end(&hash,out);return RF_OK;
}
static int scene_clutter_checkpoint_compare(const void *a,const void *b)
{uint32_t x=((const rf_clutter_checkpoint_record*)a)->uid,y=((const rf_clutter_checkpoint_record*)b)->uid;return x<y?-1:x>y;}
static int scene_clutter_checkpoint_capture(const unsigned char source[32],int32_t now,void *out,uint32_t capacity,uint32_t *written)
{
    unsigned char identity[32];rf_clutter_checkpoint_record *rows;uint32_t i,count=campaign_clutter_records.count;int status;
    status=scene_clutter_checkpoint_identity(source,identity);if(status)return status;
    rows=count?malloc(count*sizeof(*rows)):NULL;if(count&&!rows)return RF_IO;
    for(i=0;i<count;i++){
        rf_clutter_base_owner *owner=campaign_clutter_bodies[i];int32_t left=-1;
        if(campaign_clutter_damage_bindings[i].break_pending){status=RF_RANGE;goto done;}
        if(owner->state.definition->timer>=0){status=rf_timer_remaining(owner->state.definition->timer,now,&left);if(status)goto done;if(left<0)left=0;}
        rows[i]=(rf_clutter_checkpoint_record){owner->uid,(uint32_t)owner->state.class_index,owner->state.health,
            owner->state.flags&0x204002u,campaign_clutter_damage_bindings[i].killing_type,left};
    }
    if(count)qsort(rows,count,sizeof(*rows),scene_clutter_checkpoint_compare);
    status=rf_clutter_checkpoint_encode(identity,rows,count,out,capacity,written);
 done:free(rows);return status;
}
/* Caller selects fresh-only for actual restore. Validation-only staging can
 * inspect saves while damaged gameplay is running without mutating that state. */
static int scene_clutter_checkpoint_prepare(const unsigned char source[32],const void *data,uint32_t bytes,
    int32_t now,uint32_t fresh_only,scene_clutter_restore_stage **out)
{
    unsigned char identity[32];rf_clutter_checkpoint_record *rows=NULL;scene_clutter_restore_stage *stage=NULL;
    uint32_t count,i,j;int status;
    if(!out||*out||fresh_only>1)return RF_RANGE;
    status=scene_clutter_checkpoint_identity(source,identity);if(status)return status;
    status=rf_clutter_checkpoint_preflight(data,bytes,identity,&count);if(status)return status;
    if(count!=campaign_clutter_records.count)return RF_FORMAT;
    rows=count?malloc(count*sizeof(*rows)):NULL;stage=calloc(1,sizeof(*stage)+count*sizeof(*stage->entries));
    if((count&&!rows)||!stage){status=RF_IO;goto fail;}
    status=rf_clutter_checkpoint_decode(data,bytes,identity,rows,count,&count);if(status)goto fail;stage->count=count;
    for(i=0;i<count;i++){
        rf_clutter_base_owner *owner;float initial;int32_t deadline=-1;
        for(j=0;j<count&&campaign_clutter_records.items[j].uid!=rows[i].uid;j++){}
        if(j==count){status=RF_FORMAT;goto fail;}owner=campaign_clutter_bodies[j];
        initial=campaign_clutter_damage_profiles[owner->state.class_index].life;initial=initial<0?100:initial;
        if(rows[i].class_id!=(uint32_t)owner->state.class_index||rows[i].health>initial||
           (fresh_only&&((owner->state.flags&2u)||owner->state.health!=initial||campaign_clutter_damage_bindings[j].break_pending))){status=RF_FORMAT;goto fail;}
        if(rows[i].cooldown_ms>=0){status=rf_timer_set(&deadline,now,rows[i].cooldown_ms);if(status)goto fail;}
        stage->entries[i]=(scene_clutter_restore_entry){rows[i],j,owner->state.handle,deadline};
    }
    free(rows);*out=stage;return RF_OK;
 fail:free(rows);free(stage);return status;
}
static int scene_clutter_checkpoint_targets_valid(const scene_clutter_restore_stage *stage)
{
    uint32_t i;
    if(!stage)return RF_RANGE;
    /* Revalidate every target before the first write; callers keep ownership
     * stable between this check and the assignment-only publication loop. */
    for(i=0;i<stage->count;i++){
        const scene_clutter_restore_entry *e=stage->entries+i;
        if(e->slot>=campaign_clutter_records.count||!campaign_clutter_bodies[e->slot]||
           campaign_clutter_bodies[e->slot]->state.handle!=e->handle||
           !scene_clutter_damage_identity(&campaign_registry,campaign_clutter_bodies[e->slot]))return RF_FORMAT;
    }
    return RF_OK;
}
/* Called only after target validation while scene ownership is held stable. */
static void scene_clutter_checkpoint_assign(const scene_clutter_restore_stage *stage)
{
    uint32_t i;
    for(i=0;i<stage->count;i++){
        const scene_clutter_restore_entry *e=stage->entries+i;rf_clutter_base_owner *owner=campaign_clutter_bodies[e->slot];
        owner->state.health=e->row.health;owner->state.flags=(owner->state.flags&~0x204002u)|e->row.flags;
        owner->state.definition->timer=e->deadline;campaign_clutter_damage_bindings[e->slot].killing_type=e->row.killing_type;
        campaign_clutter_damage_bindings[e->slot].break_pending=0;
        if(e->row.health!=campaign_clutter_damage_profiles[e->row.class_id].life || (e->row.flags&(2u|0x4000u)))
            printf("CLUTTER_CHECKPOINT_RESTORE uid%u health%.9g flags%u cooldown%d\n",e->row.uid,(double)e->row.health,e->row.flags,e->row.cooldown_ms);
    }
}
static inline int scene_clutter_checkpoint_publish(scene_clutter_restore_stage *stage)
{
    int status=scene_clutter_checkpoint_targets_valid(stage);if(status)return status;
    scene_clutter_checkpoint_assign(stage);return RF_OK;
}
