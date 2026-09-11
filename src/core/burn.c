#include "rf/burn.h"
#include "rf/timer.h"
#include <math.h>
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
