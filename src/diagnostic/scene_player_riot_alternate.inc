#ifndef RF_SCENE_PLAYER_RIOT_ALTERNATE_INCLUDED
#define RF_SCENE_PLAYER_RIOT_ALTERNATE_INCLUDED
/* One accepted player Riot alternate contact, not held damage.
 * Descriptor +158/+15c supplies delays; BOTH modes arm actor +4c0/+4c4.
 * Player service4a281b..4a28fa coalesces expired slots into one factory call.
 * The exact authored alternate has one10ms slot and a500ms admission wait.
 * Input/cadence stay in scene.c; the shared FP/f90 owner accounts charge.
 * No release, loaded-charge or delayed debit test belongs to this owner. */
#include "rf/entity_assets.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>

enum {
    SCENE_RIOT_ALTERNATE_MODE=0x20u,
    SCENE_RIOT_ALTERNATE_FREE=0,
    SCENE_RIOT_ALTERNATE_PRESENTING=1,
    SCENE_RIOT_ALTERNATE_ARMED=2,
    SCENE_RIOT_ALTERNATE_CONTACT=3
};
typedef struct scene_riot_alternate_definition {
    int32_t weapon,delay_ms,kind;float damage;
    uint32_t fire_ticks,ready;
} scene_riot_alternate_definition;
typedef struct scene_riot_alternate_strike {
    uint64_t ticket;
    uint32_t source,mode,phase;
    int32_t weapon,due_ms,kind;float damage;
} scene_riot_alternate_strike;
typedef struct scene_riot_alternate_owner {
    scene_riot_alternate_strike strike;
    uint64_t next_ticket;
    uint32_t presenting,servicing,last_frame,has_frame;
} scene_riot_alternate_owner;
typedef struct scene_riot_alternate_backend {
    /* Pure full-handle/selection/liveness query, independent of input/ammo. */
    uint32_t (*live)(void *,uint32_t,int32_t);
    int (*present)(void *,const scene_riot_alternate_strike *);
    int (*contact)(void *,const scene_riot_alternate_strike *,uint32_t,
        const float[3],const float[3]);
} scene_riot_alternate_backend;

/* The primary loader has already verified exact primary150/600ms and the
 * sole alternate10ms authored slot. Capture the ordinary primary damage
 * getter's player60/bash, never alternate120 or electrical-kind6. */
static int scene_riot_alternate_load(const scene_riot_primary_definition *timing,
    const rf_weapon_primary_definition *primary,scene_riot_alternate_definition *out)
{
    scene_riot_alternate_definition next={0};
    if(!timing || !primary || !out)return RF_RANGE;
    if(!timing->ready || timing->weapon<0 || timing->weapon>=64 ||
       timing->delay_ms[0]!=150 || timing->delay_ms[1]!=600 ||
       primary->alt_fire_seconds!=.5f || primary->damage!=60.f ||
       primary->damage_kind!=0 || primary->burst_count!=1 ||
       primary->semi_automatic)return RF_FORMAT;
    next.weapon=timing->weapon;next.delay_ms=10;next.fire_ticks=30;
    next.damage=primary->damage;next.kind=primary->damage_kind;next.ready=1;
    *out=next;return RF_OK;
}
static void scene_riot_alternate_reset(scene_riot_alternate_owner *owner)
{
    if(!owner)return;
    memset(&owner->strike,0,sizeof(owner->strike));owner->has_frame=0;
    /* Ticket sequence and live stack reservations deliberately survive. */
}
static void scene_riot_alternate_cancel(scene_riot_alternate_owner *owner,uint32_t source)
{
    if(!owner || !source || source==UINT32_MAX)return;
    if(owner->strike.phase && owner->strike.source==source)
        memset(&owner->strike,0,sizeof(owner->strike));
}
static uint32_t scene_riot_alternate_save_pending(const scene_riot_alternate_owner *owner)
{return owner && (owner->strike.phase || owner->presenting || owner->servicing);}
static uint32_t scene_riot_alternate_contact_current(const scene_riot_alternate_owner *owner,
    const scene_riot_alternate_strike *strike)
{
    return owner && strike && owner->servicing &&
        owner->strike.phase==SCENE_RIOT_ALTERNATE_CONTACT &&
        owner->strike.ticket==strike->ticket && owner->strike.source==strike->source &&
        owner->strike.weapon==strike->weapon &&
        owner->strike.mode==SCENE_RIOT_ALTERNATE_MODE &&
        strike->mode==SCENE_RIOT_ALTERNATE_MODE &&
        owner->strike.damage==strike->damage && owner->strike.kind==strike->kind;
}
/* Caller supplies a real fire==1 admission from the shared trigger. Occupied
 * and reentrant calls never queue another pulse or erase the old one. started
 * remains true if an onset callback cancels: accepted cadence is not refunded. */
static int scene_riot_alternate_begin(scene_riot_alternate_owner *owner,
    const scene_riot_alternate_definition *definition,uint32_t source,int32_t weapon,
    uint32_t frame,const scene_riot_alternate_backend *backend,void *context,
    uint32_t *started)
{
    scene_riot_alternate_strike next={0};int status;
    if(!owner || !definition || !backend || !backend->live || !backend->present ||
       !backend->contact || !started || !source || source==UINT32_MAX)return RF_RANGE;
    *started=0;
    if(!definition->ready || definition->weapon<0 || definition->weapon>=64 ||
       weapon!=definition->weapon || definition->delay_ms!=10 ||
       definition->fire_ticks!=30 || definition->damage!=60.f || definition->kind!=0)
        return RF_FORMAT;
    if(owner->presenting || owner->servicing || owner->strike.phase)return RF_OK;
    if(!backend->live(context,source,weapon))return RF_OK;
    if(owner->next_ticket==UINT64_MAX)return RF_RANGE;
    status=rf_timer_set(&next.due_ms,scene_riot_primary_now(frame),definition->delay_ms);
    if(status)return status;
    next.ticket=++owner->next_ticket;next.source=source;next.weapon=weapon;
    next.mode=SCENE_RIOT_ALTERNATE_MODE;next.damage=definition->damage;
    next.kind=definition->kind;next.phase=SCENE_RIOT_ALTERNATE_PRESENTING;
    owner->strike=next;owner->presenting=1;*started=1;
    status=backend->present(context,&next);owner->presenting=0;
    if(owner->strike.ticket!=next.ticket ||
       owner->strike.phase!=SCENE_RIOT_ALTERNATE_PRESENTING)return status;
    if(status || !backend->live(context,source,weapon)) {
        memset(&owner->strike,0,sizeof(owner->strike));return status;
    }
    owner->strike.phase=SCENE_RIOT_ALTERNATE_ARMED;return RF_OK;
}
/* Service accepted work before a new empty/reload branch. Current aim is
 * sampled only here; release and final-cell drain cannot invalidate it.
 * No catch-up, ammo recheck, target capture or input admission. The backend
 * commits the distinct source attempt debit once for this reserved ticket. */
static int scene_riot_alternate_service(scene_riot_alternate_owner *owner,uint32_t frame,
    const float eye[3],const float direction[3],
    const scene_riot_alternate_backend *backend,void *context)
{
    scene_riot_alternate_strike strike;float contact_eye[3],contact_direction[3];
    uint32_t i;int expired,status;
    if(!owner || !eye || !direction || !backend || !backend->live ||
       !backend->contact)return RF_RANGE;
    if(owner->presenting || owner->servicing)return RF_OK;
    if(owner->strike.phase!=SCENE_RIOT_ALTERNATE_ARMED)return RF_OK;
    if(!backend->live(context,owner->strike.source,owner->strike.weapon)) {
        memset(&owner->strike,0,sizeof(owner->strike));return RF_OK;
    }
    if(owner->has_frame && owner->last_frame==frame)return RF_OK;
    for(i=0;i<3;i++)if(!isfinite(eye[i]) || !isfinite(direction[i]))return RF_RANGE;
    status=rf_timer_expired(owner->strike.due_ms,scene_riot_primary_now(frame),&expired);
    if(status)return status;
    owner->last_frame=frame;owner->has_frame=1;if(!expired)return RF_OK;
    strike=owner->strike;
    owner->strike.due_ms=-1;owner->strike.phase=SCENE_RIOT_ALTERNATE_CONTACT;
    owner->servicing=1;
    memcpy(contact_eye,eye,sizeof(contact_eye));
    memcpy(contact_direction,direction,sizeof(contact_direction));
    status=backend->contact(context,&strike,frame,contact_eye,contact_direction);
    /* Retire exactly once, also on miss/error/break/cancellation. Preserve a
     * reset callback's state; never restore the captured due time or ticket. */
    if(owner->strike.ticket==strike.ticket &&
       owner->strike.phase==SCENE_RIOT_ALTERNATE_CONTACT)
        memset(&owner->strike,0,sizeof(owner->strike));
    owner->servicing=0;return status;
}
#endif
