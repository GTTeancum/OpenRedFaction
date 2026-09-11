#ifndef RF_BURN_H
#define RF_BURN_H
#include "rf/vpp.h"
#define RF_BURN_SLOTS 8
/* Original64-byte payload; links use1..8 slot tokens,0 means no head. */
typedef struct rf_burn_record {
    uint32_t emitters[4],target;int32_t attachments[4];uint32_t voice;
    float volume;uint8_t fading,padding[3];float elapsed;uint32_t source,next,previous;
} rf_burn_record;
typedef struct rf_burn_pool {
    rf_burn_record records[RF_BURN_SLOTS];uint32_t free_head,active_head;int32_t spread_deadline;
} rf_burn_pool;
typedef struct rf_burn_release_backend {
    void (*reset_emitter)(void *context,uint32_t emitter);
    void (*free_emitter)(void *context,uint32_t emitter);
    void (*stop_voice)(void *context,uint32_t voice);
    /* Clear first entity13d8 match; only if absent, first other2d0 match.
     * Owner list traversal/order belongs to caller; token identifies this pool. */
    void (*clear_owner)(void *context,uint32_t token);
    void *context;
} rf_burn_release_backend;
/*42ed20. Nonzero low reset_only byte leaves list links unchanged; otherwise
 * token must be in the active ring and moves to the free ring. Backend and
 * pool remain alive; callbacks must not mutate pool records/links. Invalid
 * arguments/list topology fail before any callbacks or mutations. */
int rf_burn_release(rf_burn_pool *pool,uint32_t token,uint32_t reset_only,const rf_burn_release_backend *backend);
/*42e8a0: clean all8 existing payloads, rebuild free list in array order and
 * clear spread deadline. New storage must be zero-initialized before first
 * call; subsequent calls release existing resources. Source/padding survive. */
int rf_burn_pool_initialize(rf_burn_pool *pool,const rf_burn_release_backend *backend);
typedef struct rf_burn_create_backend {
    /*-1 constructs temporary descriptor;0/1 copy first/second template. */
    void (*prepare)(void *context,int32_t template_index);
    /*0: target entity exists (full word);1:40a1e0;2:immunity;3:masako.
     * Stages1..3 use the low byte. */
    uint32_t (*predicate)(void *context,uint32_t stage,uint32_t target);
    uint32_t (*attachments)(void *context,uint32_t target,int32_t indices[4]);
    /*497ca0(target,current_descriptor,0,0,1), reusing prepared descriptor. */
    uint32_t (*emitter)(void *context,uint32_t target);
    uint32_t (*sound_sample)(void *context);
    /*5056a0(sample,target position,1,173c378,0). */
    uint32_t (*play)(void *context,uint32_t target,uint32_t sample);
    void *context;
} rf_burn_create_backend;
/*42e910 orchestration. Reject/exhaustion succeeds with token0. Callback
 * storage and target identity remain stable; callbacks may modify their
 * temporary descriptor, but must not mutate pool records/links. Success
 * returns1..8; emitter0 and voiceUINT32_MAX do not abort allocation.
 * Malformed rings/arguments fail before effects; no heap allocation here. */
int rf_burn_create(rf_burn_pool *pool,uint32_t target,uint32_t source,
    const rf_burn_create_backend *backend,uint32_t *token);
/* Borrowed fields from each particle owner, offsets24,28,30,34,44,48.
 * Meaningful particle names must come from particle ownership reconstruction. */
typedef struct rf_burn_emitter_view {
    float values[6];uint8_t counter_87,active_140,padding[2];
} rf_burn_emitter_view;
typedef struct rf_burn_fade_backend {
    void (*stop_emitter)(void *context,uint32_t emitter);
    uint32_t *(*type7_flags)(void *context,uint32_t target);
    int (*entity_present)(void *context,uint32_t target);
    void (*reaction)(void *context,uint32_t target);
    void (*release)(void *context,uint32_t token);
    void *context;
} rf_burn_fade_backend;
/*42f2f0: requires four distinct valid emitter views and finite fields.
 * Calls type7_flags only after the three stop callbacks; a missing type7
 * returnsRF_NOT_FOUND instead of the original null dereference. Effects are
 * not rolled back. Stop callbacks may update emitter views; identity/storage
 * must survive until release. release may invalidate record/views on return.
 * Caller supplies current shared spread deadline and clock, not frame delta. */
int rf_burn_fade(rf_burn_record *record,rf_burn_emitter_view *const emitters[4],
    uint32_t token,int32_t deadline,int32_t now,const rf_burn_fade_backend *backend);
typedef struct rf_burn_update_backend {
    int (*owner_present)(void *context,uint32_t target);
    int (*body)(void *context,uint32_t token,rf_burn_record *record);
    const rf_burn_release_backend *release;
    void *context;
} rf_burn_update_backend;
/*42ee80 traversal and timer epilogue. body supplies the remaining attachment,
 * spread, audio and owner-damage/fade sequence. It may release the current
 * record, but may not remove/reorder other active records or reuse a released
 * token during this pass. Lookups are read-only. Missing-owner release resumes
 * saved next active token, deliberately fixing the original free-ring cycle.
 * Invalid initial topology fails before callbacks; later failures do not roll
 * back earlier effects. Timer rearms225ms at successful end of pass only. */
int rf_burn_pool_update(rf_burn_pool *pool,int32_t now,const rf_burn_update_backend *backend);
typedef struct rf_burn_owner_view {
    float class_health;uint32_t flags_810,action_824,handle;
    float position[3],velocity[3];
} rf_burn_owner_view;
typedef struct rf_burn_owner_backend {
    void (*audio)(void *context,uint32_t voice,const float position[3],const float velocity[3],float volume);
    float (*random_divisor)(void *context,float minimum,float maximum);
    /*4892c0(target,amount,-1,-1,4,0,-1,0). */
    void (*damage)(void *context,uint32_t target,float amount);
    uint32_t (*random_integer)(void *context);
    void (*fade)(void *context,uint32_t token);
    void *context;
} rf_burn_owner_backend;
/*42f1dc..42f2a2. Finite class/position/velocity/delta/record values required.
 * Identity/class/frame inputs stay stable; audio may change record state and
 * damage may change owner flags/action. Storage survives until fade returns;
 * fade may release the record. Random integer must be nonnegative signed32.
 * Errors after callbacks do not roll back. Does not perform attachment/spread. */
int rf_burn_owner_tick(rf_burn_record *record,rf_burn_owner_view *owner,
    uint32_t token,float frame_seconds,const rf_burn_owner_backend *backend);
typedef struct rf_burn_attachment_backend {
    int (*attachment)(void *context,int32_t index,float position[3]);
    /*4972a0 then4972f0 on this emitter, preserving its direction. */
    int (*move_update)(void *context,uint32_t emitter,const float position[3]);
    void *context;
} rf_burn_attachment_backend;
typedef struct rf_burn_attachment_result {float spine[3];uint32_t spread_age_eligible;} rf_burn_attachment_result;
/*42ef3e..42f0bd. Caller resolves the same model/owner for every tag.
 * Callbacks retain record/storage and attachment identity; first emitter
 * update may advance elapsed, which is re-read for the12-second gate.
 * Finite fields required. Coincident legs return RF_RANGE instead of passing
 * original NaNs to emitter0. Earlier callbacks are not rolled back; result
 * is committed only on success. No timer, world transform or spread here. */
int rf_burn_attachments(rf_burn_record *record,const rf_burn_attachment_backend *backend,
    rf_burn_attachment_result *result);
#endif
