#ifndef RF_PHYSICS_H
#define RF_PHYSICS_H
#include "rf/vpp.h"
#include "rf/random.h"
/* Original4a0406..4a046e entry decision: negative/unordered orientation up-Y10c
 * clears surface; [0,.85) retains it without a probe. Otherwise probe only
 * with an existing surface or flags18000000. Returns whether to probe.
 * NULL surface returns0. No contact query or accepted-hit mutation. */
int rf_physics_surface_probe_gate(float up_y,uint32_t flags,int32_t *surface);
/* Original49feb6..49fedf, ONLY after entering49fe40's flag4000 branch and
 * its preceding updates. Zero/unordered field1b0 clears surface unless
 * flag10000000 is set. Returns whether cleared; NULL returns0. */
int rf_physics_surface_reset_gate(float field_1b0,uint32_t flags,int32_t *surface);
typedef struct rf_physics_gravity {float acceleration,vector[3];} rf_physics_gravity;
/* Original 4a0e20: updates 5a00dc and vector 7c7058=(0,-gravity,0).
 * Does not recompute the separately initialized jump impulse. Finite signed
 * gravity is supported; invalid input preserves output. No allocation. */
int rf_physics_gravity_set(rf_physics_gravity *state,float acceleration);

typedef struct rf_physics_fallback {
    float mass;
    float center[3],radius,parameter_10;
} rf_physics_fallback;
/* Recovered fields for 49ec90's no-model, empty-sphere-list path. Caller has
 * selected sphere mode and resolved material density. No allocation, inertia
 * inversion or runtime registration; the original undefined sixth sphere word
 * is deliberately not represented. Nonfinite inputs, negative density/radius
 * and overflowing output mass return RF_RANGE without modifying output. */
int rf_physics_fallback_prepare(float density,float radius,float mass,rf_physics_fallback *result);
typedef struct rf_physics_sphere {
    float center[3],radius,parameter_10;uint32_t opaque_14;
} rf_physics_sphere;
typedef struct rf_physics_spheres {
    rf_physics_sphere *items;uint32_t count,allocated_bytes;
} rf_physics_spheres;
/* Copies ordered records into one exact-sized allocation. Budget includes the
 * owner and records, excluding allocator overhead. Empty destination required;
 * source may be released after success. Rejects nonfinite centers/radii and
 * negative radii; other field bits are preserved. Errors preserve output. */
int rf_physics_spheres_open(const rf_physics_sphere *source,uint32_t count,uint32_t budget,rf_physics_spheres *result);
void rf_physics_spheres_close(rf_physics_spheres *spheres);
typedef struct rf_physics_bounds {
    float radius,minimum[3],maximum[3];
} rf_physics_bounds;
/* 4a0cb0 rebuilds radius from local sphere centers and axis-aligned bounds
 * around the supplied world position. Empty lists yield radius zero.
 * Finite position/centers and nonnegative finite radii required; errors leave
 * output unchanged. No allocation or orientation transform is performed. */
int rf_physics_spheres_bounds(const rf_physics_sphere *source,uint32_t count,
    const float position[3],rf_physics_bounds *result);
typedef struct rf_physics_mass_tensor {
    float mass,tensor[9];
} rf_physics_mass_tensor;
/* 49ec90 existing-sphere accumulation before matrix inversion at 49edf0.
 * Starts from the supplied mass/tensor, including a negative initial mass;
 * caller selects the original mass-generation branch. Requires nonempty
 * spheres, finite inputs and nonnegative density/radii. No sphere self-inertia
 * term is added by the original. Errors leave output unchanged. */
int rf_physics_spheres_accumulate(const rf_physics_sphere *source,uint32_t count,float density,
    const rf_physics_mass_tensor *initial,rf_physics_mass_tensor *result);
/* Original 4fccf0: a zero determinant preserves the input matrix successfully.
 * Finite input required; unrepresentable intermediate/output values fail with
 * RF_RANGE, leaving output unchanged. Input and output may alias. */
int rf_physics_tensor_inverse(const float source[9],float result[9]);
/* 49cd30 world tensor update, using the original nine-float storage order.
 * Two matrix products with a float store between them. Inputs must be finite;
 * errors preserve output. Output may alias either input. */
int rf_physics_tensor_world(const float local[9],const float orientation[9],float result[9]);
/* Existing-sphere generated-mass branch including the inverse tensor step.
 * Does not copy spheres into a runtime body or initialize the other fields. */
int rf_physics_spheres_prepare(const rf_physics_sphere *source,uint32_t count,float density,
    const rf_physics_mass_tensor *initial,rf_physics_mass_tensor *result);
typedef struct rf_physics_body_parameters {
    float coefficients[3],mass,local_tensor[9],position[3],orientation[9],velocity[3],vector_78[3];
    uint32_t flags;
} rf_physics_body_parameters;
/* Only fields assigned by fresh 49f010 initialization are represented here;
 * unnamed offsets retain neutral labels until their runtime uses are recovered. */
typedef struct rf_physics_body_state {
    float coefficients[3],mass,local_tensor[9],world_tensor[9];
    float position[3],next_position[3],orientation[9],next_orientation[9];
    float velocity[3],vector_c8[3],mass_vector_d4[3],vector_e0[3],vector_ec[3];
    rf_physics_bounds bounds;
    uint32_t flags,state_124;
    float vector_138[3],scalar_144;
    int32_t reference_15c;
    uint32_t word_164,word_168;
} rf_physics_body_state;
/* Shared preparation tail49f8ea..49f925 /49fdbe..49fdf9, after predicted
 * motion and swept bounds have been prepared. Resets contact time/handle,
 * clears both force accumulators and sets body01000000; all other fields,
 * including prior contact normal and face, remain unchanged. No scheduling
 * or bounds update. NULL returns RF_RANGE. */
int rf_physics_body_prepare_contact(rf_physics_body_state *state);
/*49f8aa..49f925 /49fd84..49fdf9: rebuild swept AABB from current and
 * predicted position, expand by radius, then prepare contact. Finite bounds
 * required; errors preserve the body. Equal endpoints retain original operand
 * selection and signed-zero behavior. No movement prediction or rotation. */
int rf_physics_body_prepare_sweep(rf_physics_body_state *state);
/* Original487962..487973, after a physics body finishes its substeps:
 * publish current position, synchronize pending position, rebuild bounds,
 * set object04000000 and clear body40000000. Scheduling/removal from the
 * active-body list remain the caller's responsibility. Disjoint owners;
 * finite position/radius/bounds required; errors preserve every output. */
int rf_physics_publish_position(rf_physics_body_state *state,float published[3],uint32_t *object_flags);

typedef struct rf_physics_body {
    rf_physics_body_state state;
    rf_physics_spheres spheres;
    uint32_t allocated_bytes;
} rf_physics_body;
/* Fresh 49f010 setup (third argument zero), with pre-resolved coefficients and
 * already prepared mass/local inverse tensor. Sphere flags 0x70 select the
 * source list; otherwise it is ignored. Positive sphere parameter_10 sets
 * output flag 0x2000. One owned sphere allocation, budget includes entire body
 * and records without allocator overhead. Empty owner required; errors preserve
 * it. No geometric-model mass generation, reinitialization or entity registration. */
int rf_physics_body_open(const rf_physics_body_parameters *parameters,
    const rf_physics_sphere *source,uint32_t count,uint32_t budget,rf_physics_body *result);
void rf_physics_body_close(rf_physics_body *body);
/* Class-sphere installation after creation: replace owned records, rebuild
 * bounds and flag 0x2000, preserve all other state including mass/tensors.
 * Budget covers peak body + old records + new records (no allocator overhead).
 * Open body required. Source may alias old records; errors preserve body. */
int rf_physics_body_replace_spheres(rf_physics_body *body,
    const rf_physics_sphere *source,uint32_t count,uint32_t budget);
typedef struct rf_physics_stance_cache {
    uint32_t count;
    float centers[2][8][3]; /* standing, crouching */
    float height_difference;
} rf_physics_stance_cache;
/* 4289d0 and cleared-overhead branch of 428a60, before ground refresh:
 * copy cached class centers into existing sphere records and toggle actor
 * flag 0x400. No allocation or bounds/radius update. Standing caller must
 * first pass the clearance query; caller then refreshes ground and effects.
 * At most eight spheres; malformed arguments preserve records and flags. */
int rf_physics_stance_centers(rf_physics_spheres *spheres,const float (*centers)[3],
    uint32_t count,uint32_t *actor_flags,int crouching);
/* 428a7a..428aa7: standing clearance endpoint = public position + class
 * standing/crouching height difference + .1 on Y, rounded after both adds.
 * Caller queries with the existing crouched body, not expanded spheres. */
int rf_physics_stand_endpoint(const float position[3],float height_difference,float end[3]);
typedef struct rf_physics_stand_ops {
    int (*clearance)(void *context,const float start[3],const float end[3],uint32_t *blocked);
    uint8_t *(*player_crouch)(void *context); /* Optional lookup; NULL means no player. */
    int (*refresh_ground)(void *context);
} rf_physics_stand_ops;
/* Original428a60 orchestration. Query with the existing body and published
 * position, then (if the result low byte is clear) clear actor400, resolve and
 * clear optional player crouch byte, copy standing centers, refresh ground.
 * Ground may re-enter stance/landing and mutate the actor. Do not reapply flags
 * or centers afterwards. No speed/mode changes here. Required callbacks are
 * clearance/refresh_ground. Invalid initial arguments preserve output; callback
 * effects and mutations before a later error are not rolled back. Callbacks
 * must retain valid sphere/cache storage. stood is written only on success. */
int rf_physics_try_stand(rf_physics_spheres *spheres,const rf_physics_stance_cache *cache,
    const float published[3],uint32_t *actor_flags,const rf_physics_stand_ops *ops,
    void *context,int *stood);
/* Run (descriptor 1) path 49e400, with already transformed input and resolved
 * surface traction. Preserves caller flags/force; updates velocity and next
 * position. Repeated-pass flag 0x1000000 bypasses velocity convergence. */
/* Descriptor 2 branch of 49e400: no surface projection/traction adjustment,
 * with speed/acceleration retained through the exponential. Input is already
 * transformed by the movement descriptor. No contact query or pose commit. */
int rf_physics_climb_propose(rf_physics_body_state *state,float dt,float speed,float acceleration,
    const float input[3],const float support[3]);
int rf_physics_run_propose(rf_physics_body_state *state,float dt,float speed,float acceleration,
    float traction,const float input[3],const float normal[3],const float support[3]);
/* Falling translation at 49e8b7..49e9e6, after steering/speed limiting.
 * Caller supplies frame time, gravity and support velocity; force is vector_e0.
 * Flag 0x1000000 selects the original repeated-pass path: preserve velocity,
 * use zero acceleration. Caller manages that flag per frame. Updates velocity
 * and proposed next_position only. No contact response, bounds commit or movement-mode selection. Positive
 * mass and finite inputs required; errors preserve the state. */
int rf_physics_fall_propose(rf_physics_body_state *state,float dt,float gravity,
    const float support_velocity[3]);
/* 49e7ca..49e8b7 after class-acceleration scaling and movement transform.
 * Caller selects class speed or entity+1488 cap from flag200000. Updates X/Z
 * velocity only; repeat-pass flag1000000 preserves state. No transform,
 * gravity or position integration. Finite nonnegative scalars required. */
int rf_physics_air_steer(rf_physics_body_state *state,float dt,float air_control,
    float acceleration_limit,float speed_limit,const float world_acceleration[3]);
/* Runtime force-volume fields consumed by45cd20/4868c0, not a disk record. */
typedef struct rf_physics_force_region {
    uint32_t shape,uid,flags;
    float center[3],matrix[9],radius_squared,minimum[3],maximum[3],size[3],strength;
    uint32_t active; /* Original activation byte is the low eight bits. */
} rf_physics_force_region;
struct rf_level_force_region;
/* Original 462f60 construction after authored reads. Reorders disk matrix,
 * builds bounds/radius squared and enables the region. No allocation or world
 * registration. Finite representable geometry required; errors preserve output. */
int rf_physics_force_region_build(const struct rf_level_force_region *source,
    rf_physics_force_region *result);
struct rf_level;
typedef struct rf_physics_force_collection {
    rf_physics_force_region *items;uint32_t count,allocated_bytes;
} rf_physics_force_collection;
/* One compact owned runtime array in authored order; no retained names or disk
 * records. Empty destination required. Budget includes owner and records,
 * excludes allocator overhead/stack scratch. Absent section yields empty
 * success. Errors preserve destination; source may close afterward. */
int rf_physics_forces_open(const struct rf_level *level,uint32_t budget,rf_physics_force_collection *result);
void rf_physics_forces_close(rf_physics_force_collection *forces);
/* Original 4b9330/4ba130: ordered authored UID links change only the low
 * activation byte of the first matching region. Missing UIDs are ignored.
 * Action is 0 (off) or 1 (on); invalid arguments leave records unchanged. */
int rf_physics_forces_set_state(rf_physics_force_region *regions,uint32_t count,
    const uint32_t *uids,uint32_t uid_count,uint32_t action);
typedef struct rf_physics_force_influence {float direction[3],strength;} rf_physics_force_influence;
/* Original 486949..4869f6 after region selection/eligibility: displacement
 * uses physics position, not public query position; flags 8/4 scale by squared
 * distance, with 8 taking precedence. Flags &3 bypass body radius^2/mass.
 * No rotation, velocity, mode or cap mutation. Errors preserve output; rejects
 * nonfinite results including the original radial-at-center singularity. */
int rf_physics_force_region_influence(const rf_physics_force_region *region,
    const float physics_position[3],float body_radius,float mass,rf_physics_force_influence *result);
/* Non-0x40 actor branch 486b73..486c1c. Updates cached support velocity (+8a0),
 * NOT actor velocity. Modes 1/2 suppress incoming Y. Modes 3/8 or resolved
 * class kind 1 with attachment +1380 == -1 clamp length to strength.
 * Caller has selected an eligible actor; no dt scaling. Dirty bit is set.
 * Finite results required; errors preserve support and flags. */
int rf_physics_force_actor_carry(float support_velocity[3],uint32_t *body_flags,
    const rf_physics_force_influence *influence,uint32_t mode,uint32_t class_kind,int32_t attachment);
/* 486b1c..486b6a, after replacement velocity, fall transition and first-entry
 * effect. Stores class speed unless horizontal speed exceeds it, then speed+1;
 * sets flag 200000. No velocity mutation. Finite inputs/nonnegative class cap;
 * errors preserve cap and flags. Velocity/output storage must not overlap. */
int rf_physics_force_air_cap(const float velocity[3],float class_speed,
    float *alternate_cap,uint32_t *body_flags);
/* 4868ca..486940 eligibility with resolved query/actor/player-list predicates.
 * local_related means original 48aaf0 (object flag8 OR a player actor's +200
 * reference matches object handle), not merely the local-player pointer.
 * Presence/predicate arguments must be 0/1. Returns boolean; no mutations. */
uint32_t rf_physics_force_eligible(uint32_t body_flags,uint32_t region_present,
    uint32_t region_flags,uint32_t local_related,uint32_t actor_present,uint32_t actor_mode);
/* 4869f6..486a72: flags bits16..19 randomize force direction in a cone.
 * Uses two shared CRT draws even at zero strength/dt if nibble is nonzero.
 * Returns unclamped shake amplitude for the later player-view effect. Zero
 * nibble leaves influence/RNG unchanged and reports zero amplitude. Finite
 * nonnegative dt required; failures preserve all outputs. No view mutation. */
int rf_physics_force_turbulence(rf_physics_force_influence *influence,uint32_t flags,
    float dt,rf_random_state *random,float *shake_amplitude);
/* First enabled containing region in supplied creation order. Sphere boundary
 * is strict; boxes inclusive. Unknown shapes are skipped. UINT32_MAX means
 * no match. No allocation or force application; errors preserve index. */
int rf_physics_force_region_select(const rf_physics_force_region *regions,uint32_t count,
    const float position[3],uint32_t *index);
/* Original45ce50 impact suppression: search every region until one contains
 * the public position AND has activation low byte exactly1 and flags&0x40.
 * An earlier ordinary region does not mask a later suppressing region. Uses
 * the same strict sphere/inclusive box bounds as force selection. No allocation
 * or region mutation; finite-query contract; errors preserve suppressed. */
int rf_physics_force_suppresses_damage(const rf_physics_force_region *regions,uint32_t count,
    const float position[3],uint32_t *suppressed);
/* 49dc1d..49dcf1 velocity response: stationary non-liquid contact, flags&0x80
 * clear, non-rotating actor predicate. Normal is used as supplied. Returns
 * signed impact speed for the later damage path; does not apply damage or
 * select movement modes. Finite inputs required; errors preserve outputs. */
int rf_physics_static_contact(rf_physics_body_state *state,const float normal[3],
    const float support_velocity[3],const float contact_velocity[3],float *impact_speed);
/* 49d94c..49db78: flag-80 response for prepared non-liquid, zero-inverse-mass
 * contacts. free_tangent is the resolved 42a020 predicate; mode 1 additionally
 * clamps grounded tangential Y. direction is entity +714 in body coordinates.
 * Updates velocity and signed impact only; no damage, ownership or mode change.
 * Requires flag 80 and finite inputs. Errors preserve state and impact. */
int rf_physics_player_contact(rf_physics_body_state *state,const float normal[3],
    const float support_velocity[3],const float contact_velocity[3],const float direction[3],
    uint32_t mode,uint32_t free_tangent,float *impact_speed);
/*49dcf6..49ddef: prepared positive-inverse-mass contact response. Caller
 * resolves contacted object and player predicates (low byte). Missing object,
 * or NPC against player, returns zero impact with body/normal unchanged.
 * Mode1 flattens intermediate nonzero normal Y and normalizes. Other modes
 * retain the supplied normal. Updates only velocity, normal and signed impact;
 * does not apply damage or dispatch other inverse-mass/liquid branches.
 * Finite inputs required on the responding path; errors preserve all outputs. */
int rf_physics_dynamic_contact(rf_physics_body_state *state,float normal[3],
    const float support_velocity[3],const float contact_velocity[3],uint32_t mode,
    uint32_t object_present,uint32_t object_player,uint32_t actor_player,float *impact_speed);
/* Translation block 49ffd2..4a007c for hit fraction [0,1). Remaining time uses
 * the raw fraction; position uses the 0.05-unit separation margin unless flag
 * 0x400000 is set. Updates position and scalar_144 only; bounds, rotation and
 * response are separate. Finite inputs and nonzero displacement required.
 * Flag 0x4000 selects another original path and is rejected. Errors preserve outputs. */
int rf_physics_contact_advance(rf_physics_body_state *state,float dt,float fraction,float *remaining);
typedef struct rf_physics_ground_probe {
    float start[3],end[3];
    rf_physics_sphere sphere;
    rf_physics_bounds bounds;
    uint32_t sphere_index,query_flags;
} rf_physics_ground_probe;
/* 4a0840 preparation and 499ed0 query flags. Lowest center-Y sphere, identity
 * query orientation. Caller supplies the original falling predicate result,
 * class speed and support Y velocity. No world query or landing transition.
 * Nonempty spheres and finite nonnegative radius/dt/speed required; errors
 * preserve output. No allocation. */
int rf_physics_ground_prepare(const rf_physics_sphere *spheres,uint32_t count,
    const float next_position[3],uint32_t collision_flags,int falling,float dt,
    float class_speed,float support_y,rf_physics_ground_probe *result);
/* Numeric stationary-support landing: 4a0b31 position clamp/bounds and
 * 419901..4199fe normal class-run transition with zero support/contact velocity.
 * Caller has accepted the contact normal and resolves the movement descriptor.
 * No damage, sound, animation, contact metadata or entity registry writes. */
int rf_physics_static_land(rf_physics_body_state *state,const rf_physics_ground_probe *probe,float fraction);
/* Static support position/bounds commit at 4a0b31..4a0bfa, before landing.
 * Preserves velocity and airborne flag 0x200000; clears moving-support flag. */
int rf_physics_static_support(rf_physics_body_state *state,const rf_physics_ground_probe *probe,float fraction);
/* 4a0ae3..4a0bfa numeric support commit after caller accepts the contact and
 * resolves its object. Rising mover contact velocity Y bypasses downward-only
 * clamp. Updates moving-support flag and caller-owned entity support handle;
 * static support clears both. No object lookup, special entity rejection,
 * contact-record ownership or landing transition. Errors preserve outputs. */
int rf_physics_support_commit(rf_physics_body_state *state,const rf_physics_ground_probe *probe,
    float fraction,uint32_t moving,float contact_y,uint32_t object_handle,uint32_t *support_handle);
typedef struct rf_physics_support_contact {
    uint32_t handle;int32_t material;
} rf_physics_support_contact;
/* Accepted contact commit through4a0c05: numeric support update, then raw
 * published-position copy to object3c and material transfer to entity1380. Compact retained owner; no archive pointer.
 * Caller has accepted/resolved contact and owns material semantics. Does not
 * run landing predicates or clear material on a failed query. Outputs must be
 * disjoint; errors preserve body, published position and support together. */
int rf_physics_support_accept(rf_physics_body_state *state,const rf_physics_ground_probe *probe,
    float fraction,uint32_t moving,float contact_y,uint32_t object_handle,int32_t material,
    rf_physics_support_contact *support,float published[3]);
/* 41e370/40a420 after caller resolves the support handle. Modes 1/3 copy
 * current support velocity and set body/object wake flags. NULL resolved
 * velocity means lookup failed: preserve all outputs. Output pointers must
 * be valid and disjoint; resolved velocity may equal cached velocity.
 * No allocation, lookup, mode transition or frame scheduling. */
void rf_physics_support_refresh(uint32_t mode,const float resolved_velocity[3],
    float cached_velocity[3],uint32_t *body_flags,uint32_t *object_flags);
/* 419901..41993a landing velocity before movement-class dispatch: stored
 * (velocity + previous support) - new contact velocity, componentwise.
 * No normal projection, vertical reset, movement transition or support commit.
 * Finite inputs/results; output may alias an input and is preserved on error. */
int rf_physics_landing_velocity(const float velocity[3],const float previous_support[3],
    const float contact_velocity[3],float result[3]);
/* Prepared linear translation 49f7c3..49f89f: steering acceleration has
 * already been transformed/clamped and drag resolved by movement mode. Drag
 * and force/mass are applied even on a repeated pass; caller supplies zero
 * steering on that pass and manages force clearing. Updates velocity/next
 * position only. Finite inputs, positive mass, nonnegative dt/drag required. */
/* This branch is not selected for descriptor 1; use run_propose for run. */
int rf_physics_ground_propose(rf_physics_body_state *state,float dt,float drag,
    const float steering_acceleration[3],const float support_velocity[3]);
#endif
