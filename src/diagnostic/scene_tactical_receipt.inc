#ifndef RF_SCENE_TACTICAL_RECEIPT_INCLUDED
#define RF_SCENE_TACTICAL_RECEIPT_INCLUDED
/* Parent staging: pure 24-byte receipt operations. Scene qualification,
 * movement publication and RFNC38 transactions must surround these helpers
 * before this owner is enabled. No callbacks, allocation or global RNG. */
#define SCENE_TACTICAL_PRESENT 1u
#define SCENE_TACTICAL_RECOVERY 2u
#define SCENE_TACTICAL_MODE_SHIFT 2u /* existing combat_scripted 0, 1 or 2 */
#define SCENE_TACTICAL_STATIONARY 1u
#define SCENE_TACTICAL_RELOCATING 2u
typedef struct scene_tactical_receipt {
    uint32_t flags,phase,target;
    int32_t phase_due,recovery_due;
    rf_random_state random;
} scene_tactical_receipt;
static uint32_t scene_tactical_cursor,scene_tactical_service_frame;

static int scene_tactical_receipt_admit(const scene_tactical_receipt *r)
{
    if(!r)return RF_RANGE;
    if((r->flags&~15u)||((r->flags>>SCENE_TACTICAL_MODE_SHIFT)>2u))return RF_FORMAT;
    if(r->flags&SCENE_TACTICAL_PRESENT) {
        if(!r->target||r->target==UINT32_MAX||
           (r->phase!=SCENE_TACTICAL_STATIONARY&&r->phase!=SCENE_TACTICAL_RELOCATING)||
           r->phase_due<0||r->phase_due>RF_TIMER_PERIOD)return RF_FORMAT;
    }else if(r->phase||r->target||r->phase_due||(r->flags>>SCENE_TACTICAL_MODE_SHIFT))return RF_FORMAT;
    if(r->flags&SCENE_TACTICAL_RECOVERY) {
        if(r->recovery_due<0||r->recovery_due>RF_TIMER_PERIOD)return RF_FORMAT;
    }else if(r->recovery_due)return RF_FORMAT;
    if(!r->flags&&r->random.value)return RF_FORMAT;
    return RF_OK;
}
/* Ordinary service only. Capture never repairs or expires a receipt. */
static int scene_tactical_receipt_age(scene_tactical_receipt *r,int32_t now)
{
    scene_tactical_receipt next;int expired,status;
    status=scene_tactical_receipt_admit(r);if(status)return status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!(r->flags&SCENE_TACTICAL_RECOVERY))return RF_OK;
    next=*r;status=rf_timer_expired(next.recovery_due,now,&expired);if(status)return status;
    if(expired) {
        next.flags&=~SCENE_TACTICAL_RECOVERY;next.recovery_due=0;
        if(!next.flags)memset(&next,0,sizeof(next));
    }
    *r=next;return RF_OK;
}
/* Callback-free logical release for existing void command/reset publishers.
 * Caller already owns route teardown or replacement; never touch movement
 * here. Keep recovery, including a due recovery, until ordinary service ages
 * it. This cannot restart or extend the independent deadline and avoids
 * putting a fallible timer operation after another command's publication. */
static void scene_tactical_receipt_release(scene_tactical_receipt *r)
{
    if(!r)return;
    r->flags&=SCENE_TACTICAL_RECOVERY;r->phase=r->target=0;r->phase_due=0;
    if(!r->flags)memset(r,0,sizeof(*r));
}
/* Retire only tactical phase/route ownership. An independent pending primary
 * recovery survives target loss, explicit movement and disarming. Caller
 * retires the matching route before publishing this state. */
static int scene_tactical_receipt_cancel(scene_tactical_receipt *r,int32_t now)
{
    scene_tactical_receipt next;int status=scene_tactical_receipt_admit(r);if(status)return status;
    next=*r;status=scene_tactical_receipt_age(&next,now);if(status)return status;
    next.flags&=SCENE_TACTICAL_RECOVERY;next.phase=next.target=0;next.phase_due=0;
    if(!next.flags)memset(&next,0,sizeof(next));
    *r=next;return RF_OK;
}
/*408ac0 sets inclusive 2000..4000;409050 preserves Blind age on same-target
 * Attack. The caller invokes this for a newly accepted explicit order, or a
 * newly eligible/changed target, never an incidental same-target refresh. */
static int scene_tactical_receipt_begin(scene_tactical_receipt *r,uint32_t target,
    uint32_t combat_mode,uint32_t seed,int32_t now)
{
    scene_tactical_receipt next;int status=scene_tactical_receipt_admit(r);if(status)return status;
    if(!target||target==UINT32_MAX||combat_mode>2)return RF_RANGE;
    next=*r;status=scene_tactical_receipt_age(&next,now);if(status)return status;
    if(!next.flags)next.random.value=seed;
    status=rf_timer_set_random(&next.phase_due,now,2000,4000,&next.random);if(status)return status;
    next.flags=(next.flags&SCENE_TACTICAL_RECOVERY)|SCENE_TACTICAL_PRESENT|
        (combat_mode<<SCENE_TACTICAL_MODE_SHIFT);
    next.phase=SCENE_TACTICAL_STATIONARY;next.target=target;
    *r=next;return RF_OK;
}
/* Called only after a qualified private candidate route is ready. The
 * candidate's advanced RNG is supplied in r by the transaction owner. */
static int scene_tactical_receipt_relocate(scene_tactical_receipt *r,int32_t now)
{
    scene_tactical_receipt next;int status=scene_tactical_receipt_admit(r);if(status)return status;
    if(!(r->flags&SCENE_TACTICAL_PRESENT)||r->phase!=SCENE_TACTICAL_STATIONARY)return RF_FORMAT;
    next=*r;status=rf_timer_set_random(&next.phase_due,now,300,1299,&next.random);if(status)return status;
    next.phase=SCENE_TACTICAL_RELOCATING;*r=next;return RF_OK;
}
/*406820 returns to stationary combat and sets the independent 500ms primary
 * gate. mode7bc is the proven original boolean, not an invented crouch policy.
 * Caller stops only this owner's route before publishing next. */
static int scene_tactical_receipt_finish(scene_tactical_receipt *r,int32_t now,uint32_t mode7bc)
{
    scene_tactical_receipt next;int status=scene_tactical_receipt_admit(r);if(status)return status;
    if(!(r->flags&SCENE_TACTICAL_PRESENT)||r->phase!=SCENE_TACTICAL_RELOCATING||mode7bc>1)return RF_FORMAT;
    next=*r;
    status=rf_timer_set_random(&next.phase_due,now,mode7bc?3000:2000,mode7bc?4999:3999,&next.random);if(status)return status;
    status=rf_timer_set(&next.recovery_due,now,500);if(status)return status;
    next.flags|=SCENE_TACTICAL_RECOVERY;next.phase=SCENE_TACTICAL_STATIONARY;
    *r=next;return RF_OK;
}
#endif
