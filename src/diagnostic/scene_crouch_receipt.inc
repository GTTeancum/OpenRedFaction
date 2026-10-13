#ifndef RF_SCENE_CROUCH_RECEIPT_CHECKPOINT_INCLUDED
#define RF_SCENE_CROUCH_RECEIPT_CHECKPOINT_INCLUDED
/* Bounded continuation for the staged RFNC39 integration.
 * Physical/intent fields belong to the actor; the final world join must
 * compare them with these explicit bits. No capture-time stance repair. */
#define SCENE_CROUCH_PRESENT 1u
#define SCENE_CROUCH_STAMP 2u
typedef struct scene_crouch_receipt {
    uint32_t flags;
    int32_t switch_due,attention_due;
    rf_random_state random;
    uint32_t stamp_frame;
} scene_crouch_receipt;
typedef rf_npc_checkpoint_crouch scene_crouch_wire;
static int scene_crouch_receipt_admit(const scene_crouch_receipt *r)
{
    if(!r)return RF_RANGE;
    if(r->flags&~3u)return RF_FORMAT;
    if(!(r->flags&SCENE_CROUCH_PRESENT)){
        if(r->flags||r->switch_due||r->attention_due||r->random.value||r->stamp_frame)return RF_FORMAT;
        return RF_OK;
    }
    if(r->switch_due< -1||r->switch_due>RF_TIMER_PERIOD||
       r->attention_due< -1||r->attention_due>RF_TIMER_PERIOD||
       (!(r->flags&SCENE_CROUCH_STAMP)&&r->stamp_frame))return RF_FORMAT;
    return RF_OK;
}
/* Ordinary simulation admission only. Original402cee/402cfa initialize
 * these clocks due, not disabled. Existing policy survives target changes. */
static int scene_crouch_receipt_begin(scene_crouch_receipt *r,int32_t now,uint32_t seed)
{
    scene_crouch_receipt next={0};int status=scene_crouch_receipt_admit(r);
    if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(r->flags&SCENE_CROUCH_PRESENT)return RF_OK;
    next.flags=SCENE_CROUCH_PRESENT;next.switch_due=next.attention_due=now;
    next.random.value=seed;*r=next;return RF_OK;
}
/* Keep overdue bookkeeping within the timer period; capture never calls
 * this service and must not initialize, expire or otherwise repair state. */
static int scene_crouch_receipt_service(scene_crouch_receipt *r,int32_t now)
{
    scene_crouch_receipt next;int expired,status=scene_crouch_receipt_admit(r);
    if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!(r->flags&SCENE_CROUCH_PRESENT))return RF_OK;
    next=*r;
    status=rf_timer_expired(next.switch_due,now,&expired);if(status)return status;
    if(expired)next.switch_due=now;
    status=rf_timer_expired(next.attention_due,now,&expired);if(status)return status;
    if(expired)next.attention_due=now;
    *r=next;return RF_OK;
}
static int scene_crouch_receipt_capture(const scene_crouch_receipt *r,
    uint32_t intent,uint32_t physical,int32_t now,uint32_t frame,scene_crouch_wire *out)
{
    scene_crouch_wire w={0};int status=scene_crouch_receipt_admit(r);
    if(status)return status;
    if(!out||intent>1||physical>1||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!(r->flags&SCENE_CROUCH_PRESENT)){
        if(intent||physical)return RF_FORMAT;
        *out=w;return RF_OK;
    }
    if(physical&&!(r->flags&SCENE_CROUCH_STAMP))return RF_FORMAT;
    w.flags=1u|(intent<<1)|(physical<<2)|((r->flags&SCENE_CROUCH_STAMP)?8u:0u);
    w.switch_remaining=w.attention_remaining=-1;
    if(r->switch_due!=-1){status=rf_timer_remaining(r->switch_due,now,&w.switch_remaining);if(status)return status;}
    if(r->attention_due!=-1){status=rf_timer_remaining(r->attention_due,now,&w.attention_remaining);if(status)return status;}
    /* Disabled remains -1; elapsed active clocks are encoded due0. */
    if(r->switch_due!=-1&&w.switch_remaining<0)w.switch_remaining=0;
    if(r->attention_due!=-1&&w.attention_remaining<0)w.attention_remaining=0;
    if(w.switch_remaining>4000||w.attention_remaining>2000)return RF_FORMAT;
    w.random=r->random.value;
    if(r->flags&SCENE_CROUCH_STAMP){
        w.stamp_age=frame-r->stamp_frame;
        if(w.stamp_age>0x7fffffffu)return RF_FORMAT;
    }
    *out=w;return RF_OK;
}
/* Rebase the original float-seconds field independently of the unsigned
 * frame origin. After loading a long-lived crouch into a fresh level clock,
 * frame-age may wrap as uint32_t; converting that wrapped frame to seconds
 * would invent a future timestamp. A finite negative timestamp is the
 * correct relative past on the new clock. No live owner is touched here. */
static int scene_crouch_stamp_prepare(const scene_crouch_wire *w,
    double seconds_now,double step_seconds,uint32_t *bits)
{
    float seconds;double rebased;uint32_t result=0;
    if(!w||!bits||!isfinite(seconds_now)||seconds_now<0||
       !isfinite(step_seconds)||step_seconds<=0)return RF_RANGE;
    if(!(w->flags&8u)){
        if(w->stamp_age)return RF_FORMAT;
        *bits=0;return RF_OK;
    }
    if(!(w->flags&1u)||w->stamp_age>0x7fffffffu)return RF_FORMAT;
    rebased=seconds_now-(double)w->stamp_age*step_seconds;
    if(!isfinite(rebased)||rebased>FLT_MAX||rebased< -FLT_MAX)return RF_RANGE;
    seconds=(float)rebased;memcpy(&result,&seconds,sizeof(result));
    *bits=result;return RF_OK;
}
static int scene_crouch_receipt_prepare(const scene_crouch_wire *w,uint32_t profile,
    int32_t now,uint32_t frame,scene_crouch_receipt *out,uint32_t *intent,uint32_t *physical)
{
    scene_crouch_receipt next={0};int status;
    if(!w||!out||!intent||!physical||profile>1||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(w->flags&~15u)return RF_FORMAT;
    if(!(w->flags&1u)){
        if(w->flags||w->switch_remaining||w->attention_remaining||w->random||w->stamp_age)return RF_FORMAT;
        *out=next;*intent=*physical=0;return RF_OK;
    }
    if(!profile||w->switch_remaining< -1||w->switch_remaining>4000||
       w->attention_remaining< -1||w->attention_remaining>2000||
       ((w->flags&4u)&&!(w->flags&8u))||(!(w->flags&8u)&&w->stamp_age)||w->stamp_age>0x7fffffffu)return RF_FORMAT;
    next.flags=SCENE_CROUCH_PRESENT;next.random.value=w->random;
    next.switch_due=next.attention_due=-1;
    if(w->switch_remaining!=-1){status=rf_timer_set(&next.switch_due,now,w->switch_remaining);if(status)return status;}
    if(w->attention_remaining!=-1){status=rf_timer_set(&next.attention_due,now,w->attention_remaining);if(status)return status;}
    if(w->flags&8u){next.flags|=SCENE_CROUCH_STAMP;next.stamp_frame=frame-w->stamp_age;}
    status=scene_crouch_receipt_admit(&next);if(status)return status;
    *out=next;*intent=!!(w->flags&2u);*physical=!!(w->flags&4u);return RF_OK;
}
#endif
