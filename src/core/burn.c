#include "rf/burn.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(rf_burn_record)==64,"Burn record size");
static int backend_valid(const rf_burn_release_backend *be)
{return be && be->reset_emitter && be->free_emitter && be->stop_voice && be->clear_owner;}
static int ring_valid(const rf_burn_pool *p,uint32_t head,uint32_t *seen)
{
    uint32_t at=head,previous=0,first_previous=0;
    if(!head)return 1;
    do {
        const rf_burn_record *r;
        if(at<1 || at>RF_BURN_SLOTS || (*seen&(1u<<(at-1))))return 0;
        *seen|=1u<<(at-1);r=&p->records[at-1];
        if(at==head)first_previous=r->previous;
        else if(r->previous!=previous)return 0;
        previous=at;at=r->next;
    }while(at!=head);
    return first_previous==previous;
}
static void append(rf_burn_pool *p,uint32_t *head,uint32_t token)
{
    rf_burn_record *r=&p->records[token-1];
    if(!*head){*head=token;r->next=token;r->previous=token;}
    else {
        rf_burn_record *h=&p->records[*head-1];
        r->next=*head;r->previous=h->previous;p->records[h->previous-1].next=token;h->previous=token;
    }
}
static void reset_record(rf_burn_record *r,uint32_t token,const rf_burn_release_backend *be)
{
    uint32_t i;
    for(i=0;i<4;++i)if(r->emitters[i]) {
        be->reset_emitter(be->context,r->emitters[i]);be->free_emitter(be->context,r->emitters[i]);r->emitters[i]=0;
    }
    r->target=UINT32_MAX;for(i=0;i<4;++i)r->attachments[i]=-1;
    if(r->voice!=UINT32_MAX)be->stop_voice(be->context,r->voice);
    r->voice=UINT32_MAX;r->volume=0;r->fading=0;r->elapsed=0;
    be->clear_owner(be->context,token);
}
int rf_burn_release(rf_burn_pool *p,uint32_t token,uint32_t reset_only,const rf_burn_release_backend *be)
{
    rf_burn_record *r;uint32_t seen=0;
    if(!p || token<1 || token>RF_BURN_SLOTS || !backend_valid(be))return RF_RANGE;
    if(!(reset_only&255)) {
        if(!ring_valid(p,p->active_head,&seen) || !(seen&(1u<<(token-1))) ||
           !ring_valid(p,p->free_head,&seen))return RF_FORMAT;
    }
    r=&p->records[token-1];reset_record(r,token,be);
    if(!(reset_only&255)) {
        if(r->next==token)p->active_head=0;
        else {
            if(p->active_head==token)p->active_head=r->next;
            p->records[r->previous-1].next=r->next;p->records[r->next-1].previous=r->previous;
        }
        r->next=0;r->previous=0;append(p,&p->free_head,token);
    }
    return RF_OK;
}
int rf_burn_pool_initialize(rf_burn_pool *p,const rf_burn_release_backend *be)
{
    uint32_t i;if(!p || !backend_valid(be))return RF_RANGE;
    p->free_head=0;p->active_head=0;
    for(i=0;i<RF_BURN_SLOTS;++i){reset_record(&p->records[i],i+1,be);append(p,&p->free_head,i+1);}
    p->spread_deadline=-1;return RF_OK;
}
int rf_burn_create(rf_burn_pool *p,uint32_t target,uint32_t source,
    const rf_burn_create_backend *be,uint32_t *token)
{
    rf_burn_record *r;uint32_t seen=0,index,i,sample;
    if(!p || !be || !be->prepare || !be->predicate || !be->attachments ||
       !be->emitter || !be->sound_sample || !be->play || !token)return RF_RANGE;
    if(!ring_valid(p,p->free_head,&seen) || !ring_valid(p,p->active_head,&seen))return RF_FORMAT;
    be->prepare(be->context,-1);index=p->free_head;
    if(!index){*token=0;return RF_OK;}
    if(!be->predicate(be->context,0,target) || !(be->predicate(be->context,1,target)&255) ||
       (be->predicate(be->context,2,target)&255) || (be->predicate(be->context,3,target)&255)) {
        *token=0;return RF_OK;
    }
    r=&p->records[index-1];
    if(!(be->attachments(be->context,target,r->attachments)&255)) {
        for(i=0;i<4;++i)r->attachments[i]=-1;
        *token=0;return RF_OK;
    }
    be->prepare(be->context,0);
    for(i=0;i<3;++i)r->emitters[i]=be->emitter(be->context,target);
    be->prepare(be->context,1);r->emitters[3]=be->emitter(be->context,target);
    r->target=target;sample=be->sound_sample(be->context);r->voice=be->play(be->context,target,sample);
    r->volume=1;r->fading=0;r->elapsed=0;r->source=source;
    if(r->next==index)p->free_head=0;
    else {
        p->free_head=r->next;
        p->records[r->previous-1].next=r->next;p->records[r->next-1].previous=r->previous;
    }
    r->next=0;r->previous=0;append(p,&p->active_head,index);*token=index;return RF_OK;
}
int rf_burn_fade(rf_burn_record *r,rf_burn_emitter_view *const e[4],
    uint32_t token,int32_t deadline,int32_t now,const rf_burn_fade_backend *be)
{
    uint32_t i,j,*flags;int expired,status,release;
    static const uint32_t order[6]={4,5,2,3,0,1};
    if(!r || !e || !be || !be->stop_emitter || !be->type7_flags || !be->entity_present ||
       !be->reaction || !be->release || token<1 || token>RF_BURN_SLOTS)return RF_RANGE;
    if(!isfinite(r->elapsed) || !isfinite(r->volume))return RF_FORMAT;
    for(i=0;i<4;++i) {
        if(!e[i])return RF_RANGE;
        for(j=0;j<i;++j)if(e[i]==e[j])return RF_FORMAT;
        for(j=0;j<6;++j)if(!isfinite(e[i]->values[j]))return RF_FORMAT;
    }
    if(r->elapsed>12 && (e[0]->active_140 || e[1]->active_140 || e[2]->active_140)) {
        for(i=0;i<3;++i)be->stop_emitter(be->context,r->emitters[i]);
        flags=be->type7_flags(be->context,r->target);if(!flags)return RF_NOT_FOUND;
        *flags=(*flags&0xfffffdff)|0x100;
    }
    release=r->elapsed>17;
    if(!release) {
        status=rf_timer_expired(deadline,now,&expired);if(status)return status;
        if(!expired)return RF_OK;
        if(r->elapsed>5){e[3]->counter_87=(uint8_t)(e[3]->counter_87-1);release=e[3]->counter_87==0;}
    }
    if(release) {
        if(be->entity_present(be->context,r->target))be->reaction(be->context,r->target);
        be->release(be->context,token);return RF_OK;
    }
    for(j=0;j<6;++j) {
        uint32_t field=order[j];
        for(i=0;i<4;++i) {
            if(i==3 && (field==2 || field==3))continue;
            e[i]->values[field]*=i==3?.75f:field<2?.9f:.95f;
        }
    }
    r->volume*=.95f;return RF_OK;
}
int rf_burn_pool_update(rf_burn_pool *p,int32_t now,const rf_burn_update_backend *be)
{
    uint32_t seen=0,visited=0,at,next;int status,expired;
    if(!p || !be || !be->owner_present || !be->body || !backend_valid(be->release))return RF_RANGE;
    if(!ring_valid(p,p->active_head,&seen) || !ring_valid(p,p->free_head,&seen))return RF_FORMAT;
    status=rf_timer_expired(p->spread_deadline,now,&expired);if(status)return status;
    at=p->active_head;
    while(at) {
        rf_burn_record *r;
        if(at>RF_BURN_SLOTS || (visited&(1u<<(at-1))))return RF_FORMAT;
        visited|=1u<<(at-1);r=&p->records[at-1];next=r->next==p->active_head?0:r->next;
        if(r->emitters[0] && r->emitters[1] && r->emitters[2] && r->emitters[3]) {
            if(!be->owner_present(be->context,r->target))status=rf_burn_release(p,at,0,be->release);
            else status=be->body(be->context,at,r);
            if(status)return status;
        }
        at=next;
    }
    status=rf_timer_expired(p->spread_deadline,now,&expired);if(status)return status;
    if(expired || p->spread_deadline==-1)return rf_timer_set(&p->spread_deadline,now,225);
    return RF_OK;
}
int rf_burn_owner_tick(rf_burn_record *r,rf_burn_owner_view *owner,
    uint32_t token,float dt,const rf_burn_owner_backend *be)
{
    uint32_t i,choice;float divisor,amount,elapsed;
    if(!r || !owner || !be || !be->audio || !be->random_divisor || !be->damage ||
       !be->random_integer || !be->fade || token<1 || token>RF_BURN_SLOTS)return RF_RANGE;
    if(!isfinite(dt) || !isfinite(owner->class_health) || !isfinite(r->elapsed) || !isfinite(r->volume))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(owner->position[i]) || !isfinite(owner->velocity[i]))return RF_FORMAT;
    be->audio(be->context,r->voice,owner->position,owner->velocity,r->volume);
    if(r->fading) {
        elapsed=r->elapsed+dt;if(!isfinite(elapsed))return RF_FORMAT;
        r->elapsed=elapsed;be->fade(be->context,token);return RF_OK;
    }
    if(!(owner->flags_810&1)) {
        divisor=be->random_divisor(be->context,5,8);
        if(!isfinite(divisor) || divisor==0)return RF_FORMAT;
        amount=(float)((double)dt*((double)owner->class_health/divisor));
        if(!isfinite(amount))return RF_FORMAT;
        be->damage(be->context,owner->handle,amount);
        choice=be->random_integer(be->context);if(choice>INT32_MAX)return RF_RANGE;
        owner->action_824=choice%3==0?5:choice%3==1?14:15;
    }
    return RF_OK;
}

static int burn_attachment_point(const rf_burn_attachment_backend *be,int32_t tag,float point[3])
{
    unsigned i;int status=be->attachment(be->context,tag,point);
    if(status!=RF_OK)return status;
    for(i=0;i<3;++i)if(!isfinite(point[i]))return RF_FORMAT;
    return RF_OK;
}
int rf_burn_attachments(rf_burn_record *r,const rf_burn_attachment_backend *be,rf_burn_attachment_result *out)
{
    rf_burn_attachment_result result;float left[3],right[3],other[3],delta[3],middle[3],length;
    double magnitude,inverse;unsigned i;int status;
    if(!r || !be || !be->attachment || !be->move_update || !out)return RF_RANGE;
    if(!isfinite(r->elapsed))return RF_FORMAT;
    for(i=0;i<4;++i)if(!r->emitters[i])return RF_RANGE;
    status=burn_attachment_point(be,r->attachments[2],result.spine);if(status!=RF_OK)return status;
    status=be->move_update(be->context,r->emitters[3],result.spine);if(status!=RF_OK)return status;
    if(!isfinite(r->elapsed))return RF_FORMAT;
    result.spread_age_eligible=r->elapsed<=12;
    if(result.spread_age_eligible) {
        status=burn_attachment_point(be,r->attachments[0],left);if(status!=RF_OK)return status;
        status=burn_attachment_point(be,r->attachments[1],right);if(status!=RF_OK)return status;
        status=burn_attachment_point(be,r->attachments[3],other);if(status!=RF_OK)return status;
        for(i=0;i<3;++i)delta[i]=right[i]-left[i];
        magnitude=sqrt(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
        if(!isfinite(magnitude) || magnitude==0)return RF_RANGE;
        inverse=1.0/magnitude;length=(float)magnitude;
        for(i=0;i<3;++i) {
            float normalized=(float)(delta[i]*inverse);
            float scaled=normalized*length;
            float half=scaled*.5f;
            middle[i]=left[i]+half;
            if(!isfinite(middle[i]))return RF_FORMAT;
        }
        status=be->move_update(be->context,r->emitters[0],middle);if(status!=RF_OK)return status;
        status=be->move_update(be->context,r->emitters[1],result.spine);if(status!=RF_OK)return status;
        status=be->move_update(be->context,r->emitters[2],other);if(status!=RF_OK)return status;
    }
    *out=result;return RF_OK;
}

int rf_burn_spread(rf_burn_spread_target *head,const rf_burn_spread_target *owner,
    const float spine[3],const rf_burn_record *record,uint32_t owner_uid,
    uint32_t global_value,uint32_t limit,const rf_burn_spread_backend *be)
{
    rf_burn_spread_target *target;uint32_t visits=0,stage,i;
    if(!owner || !spine || !record || !limit || !be || !be->predicate || !be->random_divisor || !be->damage)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(spine[i]))return RF_FORMAT;
    for(target=head;target;target=target->next) {
        float delta[3],divisor,amount;double distance;
        if(visits++>=limit)return RF_RANGE;
        if(target==owner)continue;
        for(stage=0;stage<4;++stage)if((be->predicate(be->context,stage,target)&255)==1)break;
        if(stage!=4)continue;
        for(i=0;i<3;++i) {
            if(!isfinite(target->position[i]))return RF_FORMAT;
            delta[i]=target->position[i]-spine[i];
        }
        distance=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2];
        if(distance>4)continue;
        target->flags_814|=0x2000;
        divisor=be->random_divisor(be->context,5,8);
        if(!isfinite(divisor) || divisor==0 || !isfinite(target->class_health))return RF_FORMAT;
        amount=(float)(((double)target->class_health/divisor)*.25);
        if(!isfinite(amount))return RF_FORMAT;
        be->damage(be->context,target,amount,record->target,global_value,owner_uid);
    }
    return RF_OK;
}

int rf_burn_body(rf_burn_record *r,uint32_t token,const rf_burn_body_context *ctx,const rf_burn_body_backend *be)
{
    rf_burn_attachment_result attachment;float world[3];int status,expired;unsigned i;
    if(!r || token<1 || token>RF_BURN_SLOTS || !ctx || !be || !ctx->owner || !ctx->basis ||
       !ctx->spread_owner || !ctx->spread_head || !ctx->spread_deadline)return RF_RANGE;
    status=rf_burn_attachments(r,&be->attachment,&attachment);if(status!=RF_OK)return status;
    if(attachment.spread_age_eligible) {
        status=rf_timer_expired(*ctx->spread_deadline,ctx->now_ms,&expired);if(status!=RF_OK)return status;
        if(expired) {
            for(i=0;i<9;++i)if(!isfinite(ctx->basis[i]))return RF_FORMAT;
            for(i=0;i<3;++i) {
                if(!isfinite(ctx->owner->position[i]))return RF_FORMAT;
                world[i]=(float)(((double)attachment.spine[2]*ctx->basis[6+i]+
                    (double)attachment.spine[1]*ctx->basis[3+i])+(double)attachment.spine[0]*ctx->basis[i]);
                world[i]+=ctx->owner->position[i];
            }
            status=rf_burn_spread(*ctx->spread_head,ctx->spread_owner,world,r,ctx->owner_uid,
                ctx->global_value,ctx->visit_limit,&be->spread);if(status!=RF_OK)return status;
        }
    }
    return rf_burn_owner_tick(r,ctx->owner,token,ctx->frame_seconds,&be->owner);
}

int rf_burn_resolve_bones(const rf_model_name *bones,uint32_t count,int32_t indices[4])
{
    static const char *const queries[4][4]={
        {"lowerleg-l","tech- leg-l-lower",NULL,NULL},
        {"lowerleg-r","tech- leg-r-lower",NULL,NULL},
        {"spine01","spine03","tech- 1spine","tech- 1spine01"},
        {"head",NULL,NULL,NULL}};
    uint32_t group,query;int status,missing=0;
    if(!indices || (count && !bones) || count>(uint32_t)INT32_MAX)return RF_RANGE;
    for(group=0;group<4;++group) {
        int32_t index=-1;
        for(query=0;query<4 && queries[group][query];++query) {
            rf_model_name name={queries[group][query],strlen(queries[group][query])};
            status=rf_model_find_bone_substring(bones,count,name,&index);
            if(status==RF_OK)break;
            if(status!=RF_NOT_FOUND)return status;
        }
        indices[group]=index;if(index==-1)missing=1;
    }
    return missing?RF_NOT_FOUND:RF_OK;
}
