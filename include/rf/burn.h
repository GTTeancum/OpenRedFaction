#ifndef RF_BURN_H
#define RF_BURN_H
#include "rf/vpp.h"
#include "rf/model.h"
#include "rf/geometry.h"
#include "rf/particle_pool.h"
#define RF_BURN_SLOTS 8
/* Original64-byte payload; links use1..8 slot tokens,0 means no head. */
typedef struct rf_burn_record {
    uint32_t emitters[4],target;int32_t attachments[4];uint32_t voice;
    float volume;uint8_t fading,padding[3];float elapsed;uint32_t source,next,previous;
} rf_burn_record;
typedef struct rf_burn_retarget_backend {
    int (*attachments)(void *context,uint32_t target,int32_t indices[4]);
    void (*release)(void *context,uint32_t token);
    void *context;
} rf_burn_retarget_backend;
/*42f510 after40a0e0 resolution. NULL target_flags means missing object and
 * releases token without inspecting emitters. Otherwise the caller provides
 * four distinct emitter owner fields and object29c flags, all stable/disjoint.
 * Attachment refresh sees the new target; RF_NOT_FOUND retains partial bone
 * indices, unlike burn creation. Other errors return after the target commit.
 * Success retargets emitters, starts fading and sets target flag200. No actor
 * owner-field transfer or allocation. Release may invalidate the record. */
int rf_burn_retarget(rf_burn_record *record,uint32_t token,uint32_t target,
    uint32_t *target_flags,int32_t *const emitter_owners[4],const rf_burn_retarget_backend *backend);
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
typedef struct rf_burn_release_owner_backend {
    void (*stop_voice)(void *context,uint32_t voice);
    void (*clear_owner)(void *context,uint32_t token);
    void *context;
} rf_burn_release_owner_backend;
/*42ed20 with actual4973d0/497d80 resource handling: clear enable low byte,
 * detach surviving particles, return emitter slots to free tail, then stop
 * voice and clear owner. Nonzero emitter tokens are distinct active slot+1.
 * Requires intact caller-owned particle/emitter lists as for pool_release;
 * external callbacks cannot mutate these pools or burn links. No allocation. */
int rf_burn_release_resolved(rf_burn_pool *pool,uint32_t token,uint32_t reset_only,
    rf_emitter_pool *emitters,const rf_burn_release_owner_backend *backend);
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
/* Compact oracle view: min/max velocity, spawn delay and radius (original
 *24,28,30,34,44,48); counter_87 is spawn-color alpha, active_140 is enabled.
 * Live emitter storage is handled by rf_burn_fade_resolved below. */
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
typedef struct rf_burn_fade_owner_backend {
    uint32_t *(*type7_flags)(void *context,uint32_t target);
    int (*entity_present)(void *context,uint32_t target);
    void (*reaction)(void *context,uint32_t target);
    void (*release)(void *context,uint32_t token);
    void *context;
} rf_burn_fade_owner_backend;
/* Same fade sequence over four distinct active slot+1 emitter tokens. Writes
 * real velocity/delay/radius bounds and spawn-color alpha;4973d0 clears only
 * the enabled low byte. No staging copy: callbacks observe prior mutations.
 * Owner callbacks retain the pool/slots until release, which may invalidate
 * both record and slots. No allocation; previous fade callback contracts apply. */
int rf_burn_fade_resolved(rf_burn_record *record,rf_emitter_pool *emitters,
    uint32_t token,int32_t deadline,int32_t now,const rf_burn_fade_owner_backend *backend);
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
typedef struct rf_burn_spread_target {
    struct rf_burn_spread_target *next;
    float position[3],class_health;
    uint32_t flags_814,handle;
} rf_burn_spread_target;
typedef struct rf_burn_spread_backend {
    /* Stages0..3:429990,427020,40a110,4290d0; only low byte1 rejects. */
    uint32_t (*predicate)(void *context,uint32_t stage,rf_burn_spread_target *target);
    float (*random_divisor)(void *context,float minimum,float maximum);
    /*4892c0(target.handle,amount,source,global_value,4,0,owner_uid,0). */
    void (*damage)(void *context,rf_burn_spread_target *target,float amount,
        uint32_t source,uint32_t global_value,uint32_t owner_uid);
    void *context;
} rf_burn_spread_backend;
/*42f0f7..42f1dc. NULL ends the caller-owned live list; owner excluded by identity.
 * World spine/global/UID inputs are stable. Callbacks may mutate target fields
 * and links but cannot invalidate current storage before its next link is read.
 * visit_limit bounds corrupt cycles; no rollback on errors. Class and position
 * must be finite when consumed. Caller supplies original timer/age gating and
 * synchronizes persistent views with damage; this does not own entity storage. */
int rf_burn_spread(rf_burn_spread_target *head,const rf_burn_spread_target *owner,
    const float world_spine[3],const rf_burn_record *record,uint32_t owner_uid,
    uint32_t global_value,uint32_t visit_limit,const rf_burn_spread_backend *backend);
typedef struct rf_burn_body_context {
    rf_burn_owner_view *owner;
    const float *basis;
    rf_burn_spread_target *spread_owner;
    rf_burn_spread_target **spread_head;
    const int32_t *spread_deadline;
    int32_t now_ms;float frame_seconds;
    uint32_t owner_uid,global_value,visit_limit;
} rf_burn_body_context;
typedef struct rf_burn_body_backend {
    rf_burn_attachment_backend attachment;
    rf_burn_spread_backend spread;
    rf_burn_owner_backend owner;
} rf_burn_body_backend;
/* Complete live-owner42ef3e..42f2a2. Caller retains context and referenced live
 * views across callbacks; spread changes must be visible to the owner tail.
 * Reads deadline/head/basis after attachment updates. UID/global/frame stay
 * stable for this call. Inherits phase guards and no-rollback semantics;
 * fade may release record at the end. Outer traversal owns timer rearming. */
int rf_burn_body(rf_burn_record *record,uint32_t token,
    const rf_burn_body_context *context,const rf_burn_body_backend *backend);
/*42eb20 fallback order over a resolved model bone list, via51d690 substring
 * lookup. Writes all four indices, retaining partial matches on NOT_FOUND.
 * Malformed names can fail after earlier outputs; no allocation/model lookup.
 * Caller42e910 resets all indices if any required bone is missing. */
int rf_burn_resolve_bones(const rf_model_name *bones,uint32_t count,int32_t indices[4]);
typedef struct rf_burn_attachment_runtime {
    const float (*pose)[12];uint32_t bone_count;
    rf_emitter_pool *emitters;
    const rf_geometry_collision_world *world;
    const rf_particle_emitter_parent *parent;
    rf_random_state *random;
    int32_t owner,now_ms;uint32_t parent_room,global_enabled;float frame_seconds;
} rf_burn_attachment_runtime;
/* Concrete attachment backend for42ef3e..42f0bd: evaluated bone query,4972a0
 * cached-room movement, then4972f0 update. Burn emitter tokens are slot+1.
 * All four slots must be active and belong to owner. Parent is the stable
 * resolved owner view (NULL represents failed resolution); pose is evaluated
 * already. Local bone coordinates are passed unchanged to original movement;
 * emission applies its own parent transform and subsequent room inheritance.
 * Room tokens are world index+1. World/pose/pools/parent/RNG stay alive and
 * cannot alias output. No allocation; serialize shared world scratch.
 * Errors after placement starts may leave earlier moves/emissions committed.
 * This does not create owners, evaluate animation, or schedule a burn pass. */
int rf_burn_attachments_resolved(rf_burn_record *record,
    const rf_burn_attachment_runtime *runtime,rf_burn_attachment_result *result);
#endif
