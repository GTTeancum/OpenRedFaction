#include "rf/burn.h"
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
