#ifndef RF_SCENE_PLAYER_RIOT_PRIMARY_INCLUDED
#define RF_SCENE_PLAYER_RIOT_PRIMARY_INCLUDED
/* Player primary timer owner. Action runtime remains unverified.
 * RF.exe4a579b..4a57f8 arms both player timers from one admission time.
 * 4a281b..4a2848 clears ALL expired timers before ONE4a28fa factory call.
 * NPC409340 differs: its factory call40956b is inside the two-timer loop.
 * This owner never admits a trigger, debits charge, or changes cadence/alt.
 * Caller owns current-pose collision and explicit live cancellation policy.
 */
#include "rf/entity_assets.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>

enum {
    SCENE_RIOT_PRIMARY_FREE=0,
    SCENE_RIOT_PRIMARY_PRESENTING=1,
    SCENE_RIOT_PRIMARY_ARMED=2,
    SCENE_RIOT_PRIMARY_CONTACT=3
};
typedef struct scene_riot_primary_definition {
    int32_t weapon,delay_ms[2];uint32_t ready;
} scene_riot_primary_definition;
typedef struct scene_riot_primary_strike {
    uint64_t ticket;
    uint32_t source,remaining,phase;
    int32_t weapon,due_ms[2];
} scene_riot_primary_strike;
typedef struct scene_riot_primary_owner {
    scene_riot_primary_strike strike;
    uint64_t next_ticket;
    uint32_t presenting,servicing,last_frame,has_frame;
} scene_riot_primary_owner;
typedef struct scene_riot_primary_backend {
    /* Pure query: exact full-handle registry/entity identity, living owned
     * selected Riot, on foot, no reload/alternate takeover. No callbacks,
     * animation advancement, target selection, or scene mutation. */
    uint32_t (*live)(void *,uint32_t source,int32_t weapon);
    /* Exactly one existing attack counter/action/audio onset. No contact.
     * A cancellation/reset from a presentation callback cannot re-arm it. */
    int (*present)(void *,const scene_riot_primary_strike *);
    /* ONE existing primary contact attempt, at the supplied current pose.
     * due_mask is diagnostic only: NEVER multiply damage/calls by its bits.
     * Do not retain these stack pointers. Damage/audio/death may cancel/reset.
     * After callbacks, use contact_current plus live before any continuation;
     * service's outer revalidation cannot guard code inside this callback. */
    int (*contact)(void *,const scene_riot_primary_strike *,uint32_t frame,
        const float eye[3],const float direction[3],uint32_t due_mask);
} scene_riot_primary_backend;

/* Startup metadata only; named lookup binds the actual catalog ID. Failed
 * admission preserves output. Alt metadata is checked, never scheduled here.
 * Fixed150/600ms are admitted from the exact authored floats, not substitutes
 * for missing metadata. The shared loader owns its temporary scratch block. */
static int scene_riot_primary_load(rf_vpp *tables,
    const rf_weapon_names *names,scene_riot_primary_definition *out)
{
    rf_weapon_impact_delays delays;scene_riot_primary_definition next={0};
    int status;
    if(!tables || !names || !out || names->count>64 ||
       names->primary_count>names->count)return RF_RANGE;
    next.weapon=rf_weapon_name_find(names,"Riot Stick");
    if(next.weapon<0)return RF_NOT_FOUND;
    if((uint32_t)next.weapon>=names->primary_count)return RF_FORMAT;
    status=rf_weapon_impact_delays_load(tables,"Riot Stick",128*1024,&delays);
    if(status)return status;
    if(delays.primary_count!=2 || delays.primary_seconds[0]!=.15f ||
       delays.primary_seconds[1]!=.6f || delays.alt_count!=1 ||
       delays.alt_seconds[0]!=.01f)return RF_FORMAT;
    next.delay_ms[0]=150;next.delay_ms[1]=600;next.ready=1;
    *out=next;return RF_OK;
}

/* Deliberately preserve monotonically increasing tickets and callback guards.
 * Use a zero-initialized owner for initial construction; never memset a live
 * owner in a callback or on timeline replacement. */
static void scene_riot_primary_reset(scene_riot_primary_owner *owner)
{
    if(!owner)return;
    memset(&owner->strike,0,sizeof(owner->strike));owner->has_frame=0;
}
static void scene_riot_primary_cancel(scene_riot_primary_owner *owner,uint32_t source)
{
    if(!owner || !source || source==UINT32_MAX)return;
    if(owner->strike.phase && owner->strike.source==source)
        memset(&owner->strike,0,sizeof(owner->strike));
}
static uint32_t scene_riot_primary_save_pending(const scene_riot_primary_owner *owner)
{
    return owner && (owner->strike.phase || owner->presenting || owner->servicing);
}
/* For the parent's contact adapter after any reentrant scene callback.
 * An already-applied effect is not rolled back; stop stale continuation and
 * never reuse cached target pointers across a callback without revalidation. */
static uint32_t scene_riot_primary_contact_current(const scene_riot_primary_owner *owner,
    const scene_riot_primary_strike *strike)
{
    return owner && strike && owner->servicing &&
        owner->strike.phase==SCENE_RIOT_PRIMARY_CONTACT &&
        owner->strike.ticket==strike->ticket && owner->strike.source==strike->source &&
        owner->strike.weapon==strike->weapon;
}
static int32_t scene_riot_primary_now(uint32_t frame)
{return (int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);}

/* Call only for an ALREADY accepted primary fire==1. combat_trigger remains
 * the sole input/admission/cadence owner. started is also1 when presentation
 * cancels the accepted swing: never retry it or refund its accepted cadence.
 * Occupied/reentrant calls return OK/started0 and leave the old owner intact.
 */
static int scene_riot_primary_begin(scene_riot_primary_owner *owner,
    const scene_riot_primary_definition *definition,uint32_t source,int32_t weapon,
    uint32_t frame,const scene_riot_primary_backend *backend,void *context,
    uint32_t *started)
{
    scene_riot_primary_strike next={0};int32_t now;int status;
    if(!owner || !definition || !backend || !backend->live ||
       !backend->present || !backend->contact || !started ||
       !source || source==UINT32_MAX)return RF_RANGE;
    *started=0;
    if(!definition->ready || definition->weapon<0 || definition->weapon>=64 ||
       definition->delay_ms[0]!=150 || definition->delay_ms[1]!=600 ||
       weapon!=definition->weapon)return RF_FORMAT;
    if(owner->presenting || owner->servicing || owner->strike.phase)return RF_OK;
    if(!backend->live(context,source,weapon))return RF_OK;
    if(owner->next_ticket==UINT64_MAX)return RF_RANGE;
    now=scene_riot_primary_now(frame);
    status=rf_timer_set(next.due_ms,now,definition->delay_ms[0]);if(status)return status;
    status=rf_timer_set(next.due_ms+1,now,definition->delay_ms[1]);if(status)return status;
    next.ticket=++owner->next_ticket;next.source=source;next.weapon=weapon;
    next.remaining=3;next.phase=SCENE_RIOT_PRIMARY_PRESENTING;
    owner->strike=next;owner->presenting=1;*started=1;
    status=backend->present(context,&next);
    owner->presenting=0;
    if(owner->strike.ticket!=next.ticket ||
       owner->strike.phase!=SCENE_RIOT_PRIMARY_PRESENTING)return status;
    if(status || !backend->live(context,source,weapon)) {
        memset(&owner->strike,0,sizeof(owner->strike));return status;
    }
    owner->strike.phase=SCENE_RIOT_PRIMARY_ARMED;return RF_OK;
}

/* Call every actual combat simulation frame, before equipment early returns,
 * after current ownership/health/control cancellation is known. No wall clock,
 * input-held test, target capture, catch-up loop, or new attack admission.
 * A live check still runs on a duplicate frame so death/unequip cannot leave a
 * stale pending swing. The frame guard bounds contact dispatch to one/frame.
 */
static int scene_riot_primary_service(scene_riot_primary_owner *owner,uint32_t frame,
    const float eye[3],const float direction[3],
    const scene_riot_primary_backend *backend,void *context)
{
    scene_riot_primary_strike strike;uint32_t i,due=0;int expired,status=RF_OK;
    float contact_eye[3],contact_direction[3];int32_t now;
    if(!owner || !eye || !direction || !backend || !backend->live ||
       !backend->contact)return RF_RANGE;
    if(owner->presenting || owner->servicing)return RF_OK;
    if(owner->strike.phase!=SCENE_RIOT_PRIMARY_ARMED)return RF_OK;
    if(!backend->live(context,owner->strike.source,owner->strike.weapon)) {
        memset(&owner->strike,0,sizeof(owner->strike));return RF_OK;
    }
    if(owner->has_frame && owner->last_frame==frame)return RF_OK;
    for(i=0;i<3;i++)if(!isfinite(eye[i]) || !isfinite(direction[i]))return RF_RANGE;
    now=scene_riot_primary_now(frame);
    for(i=0;i<2;i++)if(owner->strike.remaining&(1u<<i)) {
        status=rf_timer_expired(owner->strike.due_ms[i],now,&expired);if(status)return status;
        if(expired)due|=1u<<i;
    }
    owner->last_frame=frame;owner->has_frame=1;
    if(!due)return RF_OK;
    strike=owner->strike;
    /* Both due bits retire before ONE contact callback, including a miss,
     * blocked ray, protected target, failed callback, or cancellation. */
    owner->strike.remaining&=~due;
    for(i=0;i<2;i++)if(due&(1u<<i))owner->strike.due_ms[i]=-1;
    owner->strike.phase=SCENE_RIOT_PRIMARY_CONTACT;owner->servicing=1;
    memcpy(contact_eye,eye,sizeof(contact_eye));
    memcpy(contact_direction,direction,sizeof(contact_direction));
    status=backend->contact(context,&strike,frame,contact_eye,contact_direction,due);
    /* Keep the stack reservation even when contact reset/cleared its record.
     * No copied mask/deadlines are restored after callbacks. */
    if(owner->strike.ticket==strike.ticket &&
       owner->strike.phase==SCENE_RIOT_PRIMARY_CONTACT) {
        if(status || !owner->strike.remaining ||
           !backend->live(context,strike.source,strike.weapon))
            memset(&owner->strike,0,sizeof(owner->strike));
        else owner->strike.phase=SCENE_RIOT_PRIMARY_ARMED;
    }
    owner->servicing=0;return status;
}
#endif
