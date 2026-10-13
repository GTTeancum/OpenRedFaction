#ifndef RF_SCENE_PURSUIT_DELAY_RECEIPT_INCLUDED
#define RF_SCENE_PURSUIT_DELAY_RECEIPT_INCLUDED
#include "scene_pursuit_sight_clock.inc"
#define SCENE_PURSUIT_DELAY_PRESENT 1u
#define SCENE_PURSUIT_DELAY_MODE_SHIFT 1u
typedef struct scene_pursuit_delay_receipt {
    uint32_t flags,target;
    scene_pursuit_sight_clock sight;
} scene_pursuit_delay_receipt;
static int scene_pursuit_delay_admit(const scene_pursuit_delay_receipt *r)
{
    if(!r)return RF_RANGE;
    if((r->flags&~7u)||(r->flags>>SCENE_PURSUIT_DELAY_MODE_SHIFT)>2u)return RF_FORMAT;
    if(!(r->flags&SCENE_PURSUIT_DELAY_PRESENT))
        return r->flags||r->target||r->sight.flags||r->sight.due||r->sight.random.value?RF_FORMAT:RF_OK;
    if(!r->target||r->target==UINT32_MAX)return RF_FORMAT;
    return scene_pursuit_sight_admit(&r->sight);
}
/* Called only at accepted live target assignment or first legacy admission.
 * Deliberate bounded adaptation: a new target begins constructor-disarmed;
 * same-target orders retain sight history, deadline and private RNG. */
static int scene_pursuit_delay_begin(scene_pursuit_delay_receipt *r,
    uint32_t target,uint32_t mode,uint32_t seed)
{
    scene_pursuit_delay_receipt next={0};int status=scene_pursuit_delay_admit(r);if(status)return status;
    if(!target||target==UINT32_MAX||mode>2)return RF_RANGE;
    if((r->flags&SCENE_PURSUIT_DELAY_PRESENT)&&r->target==target)next=*r;
    else {next.target=target;next.sight.random.value=seed;}
    next.flags=SCENE_PURSUIT_DELAY_PRESENT|(mode<<SCENE_PURSUIT_DELAY_MODE_SHIFT);
    *r=next;return RF_OK;
}
#endif
