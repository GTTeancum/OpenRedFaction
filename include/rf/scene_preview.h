#ifndef RF_SCENE_PREVIEW_H
#define RF_SCENE_PREVIEW_H
#include "rf/material.h"
#include "rf/preview.h"
#include "rf/entity.h"
#include "rf/random.h"
#include "rf/event.h"
#include "rf/eye.h"
#include "rf/player.h"
#include "rf/collision.h"
#include "rf/glare.h"

/* Play a resolved48a9c0 request from an already resident campaign sample.
 * Flat pan is the original float bit pattern retained in request.pan; values
 * outside the supported device range [-10,10] are rejected, not clamped.
 * Category0 is supported with unity settings; other categories fail explicitly.
 * Returns a port device voice ID; does not assign entity808 or load PCM.
 * Errors preserve the output ID. Listener updates affect positional voices only. */
int rf_scene_sound_play_request(const rf_player_sound_request *request,int32_t *voice);
/* Registered campaign player damage sound in the current first-person profile.
 * Retains4196f0 cooldown/voice and class sound groups, shares caller RNG, lazily
 * loads bounded PCM and dispatches nonpositional playback. Does not update808.
 * DeathSnd uses the retained same-class descriptor and marks flags810 bit4 once.
 * Other camera profiles return NOT_FOUND. Full death transitions remain separate.
 * Caller supplies the pain fraction and current timer; not attached to hazards. */
int rf_scene_player_pain_sound(uint32_t handle,float fraction,int32_t now,rf_random_state *random);

/* Borrowed services for runtime damage events targeting registered NPCs/player.
 * Caller updates clock/difficulty and supplies complete synchronous reactions.
 * A fresh status/dispatch count is required for each outer dispatch. The first
 * failure is retained; subsequent lookups stop dispatch. Check services.status
 * after every outer dispatch because damage callbacks return void. now_ms is
 * the camera timer clock, distinct from the float-time bits used by damage.
 * Other target families remain unsupported. No owner removal in callbacks. */
typedef struct rf_scene_event_damage_services {
    const rf_damage_effect_backend *effects;
    float difficulty;uint32_t clock_bits;
    int status;uint32_t dispatches;float last_amount;int32_t now_ms;
    rf_random_state *player_pain_random; /* Optional borrowed shared RNG for live player pain. */
} rf_scene_event_damage_services;
typedef rf_scene_event_damage_services rf_scene_npc_event_damage_services;
int rf_scene_event_damage_bind(rf_scene_event_damage_services *services,rf_event_damage_backend *backend);
int rf_scene_npc_event_damage_bind(rf_scene_npc_event_damage_services *services,rf_event_damage_backend *backend);
typedef struct rf_scene_npc_pain_ops {
    int (*reset_weapon)(void *context,uint32_t handle,int32_t weapon);
    int (*play_sound)(void *context,uint32_t handle,const char *class_name);
    void *context;
} rf_scene_npc_pain_ops;
/* Registered startup NPC pain binding; caller owns clock/RNG invocation order.
 * Uses retained timers, base action mappings and live playback. Current NPC
 * views have no associated player/attachment owners. Reached weapon/sound
 * operations require callbacks; NULL is allowed only when neither is reached.
 * Callbacks keep the actor and catalog alive and must not replace its mappings.
 * Missing target returns NOT_FOUND. Errors retain preceding effects. */
int rf_scene_npc_pain(uint32_t handle,int32_t now,rf_random_state *random,const rf_scene_npc_pain_ops *ops);
/* Retained nonlethal NPC sound adapter; registered NPCs have no player owner.
 * Uses the caller's shared RNG, cached eye and pain groups, and bounded lazy
 * waveform residency. Death/player override owners remain unsupported.
 * Errors after dispatch preserve prior timer/RNG/loading effects. */
int rf_scene_npc_pain_sound(uint32_t handle,float fraction,int32_t now,rf_random_state *random);
/* Damage's mode0 AI notification: missing/stale/self sources are verified
 * original no-ops. A live different source requires the unfinished AI owner
 * and returns NOT_FOUND. This does not implement forced mode1 alerts. */
int rf_scene_npc_damage_ai(uint32_t handle,uint32_t source);
/* Run the verified SP tail on a registered NPC's retained death fields.
 * Name is the caller's resolved event-name token. Publish timer/model before
 * resource callbacks and reload afterward; view.flags810 is authoritative and
 * mirrored into the damage view. Callback owners/registration remain alive.
 * This alone must not be used to dispatch a complete death. */
int rf_scene_npc_death_tail(uint32_t handle,uint32_t name,const rf_entity_death_tail_backend *);
/*41fdc0 entry on a registered NPC: command/velocity clearing and dying flags.
 * Already-dying is a complete no-op with entered=0; errors preserve owners
 * and entered. Does not dispatch collision teardown or later death stages. */
int rf_scene_npc_death_entry(uint32_t handle,uint32_t *entered);
/* Snapshot current registered kind0 NPC owners for pair processing. Model token
 * follows publication/transfer; position is published, forward is the current
 * authored NPC orientation. No allocation, pair scheduling or response effects.
 * Caller refreshes after mutations. Failure preserves output. */
int rf_scene_npc_collision_view(uint32_t handle,rf_collision_pair_actor_state *result);
/* Registered active NPC427450 speed request and42a580 animation request.
 * Speed uses retained class settings/mass and actor75c attachment
 * (-1 means absent). SP only. Motion uses the retained authored mapping and
 * controller, including original fallback/retarget behavior. Neither ticks AI,
 * animation or physics. Stale/corpse owners and errors preserve actor state. */
/*4281a0: wake body, install class-selected fall descriptor and stable identity
 * movement orientation. Preserves position, velocity and support ownership.
 * Registered active NPC only; errors preserve state. No physics stepping. */
int rf_scene_npc_fall(uint32_t handle);
int rf_scene_npc_set_speed(uint32_t handle,int32_t requested);
int rf_scene_npc_request_motion(uint32_t handle,int32_t requested,float duration);
/*4a0840 NPC query preparation bound to retained class/body/support and current
 * world/mover geometry. Outputs a probe and original-layout contact payload;
 * geometry face tokens remain port tokens. Misses set time1/reserved_1ec0 and
 * preserve other contact fields. Errors preserve all outputs. No support
 * acceptance or landing effects. Player-flag owners require a separate path. */
int rf_scene_npc_ground_query(const rf_geometry_collision_world *world,uint32_t handle,float elapsed,
    rf_physics_ground_probe *probe,rf_collision_actor_contact *contact,uint32_t *matched);


typedef struct rf_scene_npc_stance_services {
    uint8_t *(*player_crouch)(void *,uint32_t handle);
    int (*refresh_ground)(void *,uint32_t handle);
    void *context;
} rf_scene_npc_stance_services;
/*428a60 with retained NPC spheres/class stance cache and world/mover clearance.
 * Ground callback is required and may reenter stance. Effects already applied
 * survive callback errors; stood changes only on success. Borrowed owners and
 * caches must remain alive. No speed/mode change or automatic scheduling. */
int rf_scene_npc_try_stand(const rf_geometry_collision_world *world,uint32_t handle,
    const rf_scene_npc_stance_services *services,int *stood);
/*4280b0: blocked standing stops the transition; successful walking clears the previous region. */
int rf_scene_npc_normal(const rf_geometry_collision_world *world,uint32_t handle,
    const rf_scene_npc_stance_services *services);
extern uint32_t rf_scene_npc_normal_test[4];
extern uint32_t rf_scene_navigation[6];
extern uint32_t rf_scene_navigation_workspace[4];
extern uint32_t rf_scene_clutter[8];
extern uint32_t rf_scene_clutter_render[8];
extern uint32_t rf_scene_clutter_materials[8];
extern uint32_t rf_scene_clutter_skins[6];
extern uint32_t rf_scene_clutter_bodies[10];
extern uint32_t rf_scene_clutter_collision[9];
extern uint32_t rf_scene_clutter_tags[4];
extern uint32_t rf_scene_clutter_tag_queries[5];
extern uint32_t rf_scene_glare_resources[9];
extern uint32_t rf_scene_glare_instances[10];
/* Registered prop tag services; placement consumes current owned pose.
 * Output preserved for missing/stale handle, missing tag or invalid geometry. */
int rf_scene_clutter_tag_find(uint32_t handle,rf_model_name query,int32_t *index);
int rf_scene_clutter_tag_place(uint32_t handle,int32_t index,float transform[12]);
/* Registered static prop model query through5031f0/static geometry. Caller
 * supplies original query pose/flags and finite disjoint input; mutable local
 * scratch and hit follow the recovered query contract. No physics scheduling. */
int rf_scene_clutter_collision_query(uint32_t handle,rf_collision_model_part_query *query,
    rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted);
/* Borrow registered prop geometry for visibility; no allocation or render marker mutation.
 * Stale handles/model identities fail without publishing an output. */
int rf_scene_clutter_visibility_view(uint32_t handle,rf_glare_visibility_object *result);
int rf_scene_clutter_visibility_model(void *context,const rf_collision_visibility_object *object,
    rf_collision_model_part_query *query,rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted);
extern uint32_t rf_scene_clutter_draw[6];
extern uint32_t rf_scene_clutter_render_dispatch[6];
/* Registered moving-solid view; cached owner handle and solid query token differ.
 * Borrow current public pose; no allocation or render flag fabrication. */
int rf_scene_mover_visibility_view(uint32_t handle,rf_glare_visibility_object *result);
extern uint32_t rf_scene_mover_visibility[3];
/* Scene-owned glare cache search, current diagnostic owner order; no drawing. */
int rf_scene_glare_visibility_pass(const float camera[3]);
extern uint32_t rf_scene_glare_search[8];
extern uint32_t rf_scene_corona_draw[8];
extern uint32_t rf_scene_volume_draw[8];
extern uint32_t rf_scene_volume_test_enabled,rf_scene_volume_test[8];
extern uint32_t rf_scene_volume_npc_test[8];
extern uint32_t rf_scene_glare_rooms[8];
extern uint32_t rf_scene_attachments[8];
extern uint32_t rf_scene_glare_retirement[8];
extern uint32_t rf_scene_glare_loss_test_enabled,rf_scene_glare_loss_test[8];
extern uint32_t rf_scene_attachment_motion[8];
/* Registered glare room token: zero absent, otherwise authored room index+1. */
int rf_scene_glare_room(uint32_t handle,uint32_t *room);
/*428030: blocked standing still selects slow mode; forced crouch sets only the flag.
 * Required standing services only when attempting to stand; borrowed owners must survive callbacks. */
int rf_scene_npc_slow(const rf_geometry_collision_world *world,uint32_t handle,
    uint32_t forced_crouch,const rf_scene_npc_stance_services *services);
extern uint32_t rf_scene_npc_slow_test[4];
extern uint32_t rf_scene_npc_stand_test[7]; /* cases,clear,blocked,ground queries,hash,errors,rejected no-cache */
typedef struct rf_scene_npc_crouch_services {
    int (*refresh_ground)(void *,uint32_t handle);
    uint32_t (*clock_bits)(void *); /* Original raw6460f0, read AFTER ground. */
    void *context;
} rf_scene_npc_crouch_services;
/*4289d0 retained NPC crouch centers/flag, required ground callback, then7b4
 * clock publication. Ground may reenter stance; its changes survive. Errors
 * stop without undoing applied state or writing the final clock. */
int rf_scene_npc_crouch(uint32_t handle,const rf_scene_npc_crouch_services *services);
extern uint32_t rf_scene_npc_crouch_test[7]; /* cases,ground,clock,expected errors,hash,errors,no-cache */

/*41e370 support refresh for one registered NPC: modes1/3 resolve retained
 * NPC/player/mover body velocity and update support velocity plus wake flags.
 * Missing/unsupported support preserves the cached velocity and wake flags.
 * No support selection, contact response, landing or position integration. */
int rf_scene_npc_refresh_support(uint32_t handle);

/* Query registered NPC spheres using an explicit proposed body and current world/
 * mover geometry. Caller scratch has at least sphere-count records. No actor
 * mutation, contact publication or scheduling. Misses preserve hit; errors
 * preserve hit/matched. Scratch may change. Zero translation returns a miss. */
int rf_scene_npc_body_sweep(const rf_geometry_collision_world *world,uint32_t handle,
    const rf_physics_body_state *proposal,uint32_t flags,rf_collision_body_sphere *scratch,
    uint32_t capacity,rf_geometry_body_hit *hit,uint32_t *matched);

/* Borrow registered NPC body/spheres for immediate response processing. Uses
 * body current/predicted transforms, not published rendering position. Owners
 * must remain stable. No response scheduling or extra-velocity lookup here. */
int rf_scene_npc_collision_response(uint32_t handle,rf_collision_actor_general_response *result);
/* Publish contact and flags even when a response reports no hit: general
 * deferral can still mutate flags/time. Other owner fields remain untouched.
 * Stale handles, unavailable models/bodies and errors preserve outputs. */
int rf_scene_npc_collision_publish(uint32_t handle,uint32_t body_flags,const rf_collision_actor_contact *contact);
/* Reserved model identity for the current player animation owner, separate from
 * NPC slot+1 tokens. Valid only during the placed stream's frame callbacks. */
#define RF_SCENE_PLAYER_MODEL UINT32_MAX
/* Registered campaign player snapshot; requires live body/model publication.
 * Uses published object position and body orientation, not the camera pose.
 * Failure preserves output. Full player model lifecycle remains separate. */
int rf_scene_player_collision_view(uint32_t handle,rf_collision_pair_actor_state *result);
/* Same immediate borrowed-body/contact contract as the NPC APIs, gated by
 * current campaign player registration and borrowed model publication. */
int rf_scene_player_collision_response(uint32_t handle,rf_collision_actor_general_response *result);
int rf_scene_player_collision_publish(uint32_t handle,uint32_t body_flags,const rf_collision_actor_contact *contact);
/* Response426fc0-style registered kind0 actor8a0 lookup. Player aliases its
 * existing support velocity; NPCs retain constructor-zeroed vectors. Model
 * publication is not required. Other actor families and stale handles return
 * NULL. Context is unused; no mutation or support refresh is performed. */
const float *rf_scene_collision_extra_velocity(void *context,uint32_t handle);
/* Execute one already-classified registered actor pair and publish both
 * contacts/flags, including deferrals returning changed=0. normal_mode is
 * exactly0 (49a420) or1 (49ab00), selected by the caller's48ca60 dispatch.
 * Stable distinct owners, finite geometry and positive masses as required by
 * the response functions. No pair creation, impulse, damage or scheduling.
 * Invalid owners/arguments preserve changed and both live owners. */
int rf_scene_actor_pair_response(uint32_t first,uint32_t second,uint32_t normal_mode,uint32_t *changed);
extern uint32_t rf_scene_collision_views[8]; /* Pointer-free live actor snapshot replay evidence. */
extern uint32_t rf_scene_collision_responses[6]; /* Live body/contact snapshot replay evidence. */
extern uint32_t rf_scene_actor_pair_test_enabled,rf_scene_actor_pair_test[8]; /* Opt-in restored-state publication fixture. */

/* Query the registered model's evaluated pose and initial selected skeletal
 * LOD. Caller supplies the104-byte query transform/scratch. Shared scene scratch
 * is synchronous/non-reentrant. Stale stationary NPC poses evaluate on demand
 * without advancing playback. Corpses require explicit displacement-aware
 * evaluation first. Sampling failure may leave a partial pose. No contact publication. */
int rf_scene_model_collision_query(uint32_t model_slot,rf_collision_model_part_query *query,
    rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted);
/* Compose49afe0 with5031f0 and registered skeletal scene geometry. First is
 * a retained player/NPC, target a retained NPC model. Publishes both contacts
 * only after successful queries; preserves changed on failure. Evaluated pose
 * required. No projectile views, pair scheduling or velocity stepping. */
int rf_scene_actor_model_response(uint32_t first,uint32_t target,uint32_t *changed);
/* Run48ca60 over a bounded caller-owned list of pair records. Endpoints are
 * registered player/NPC entity-view pointers. No allocation or pair discovery.
 * Normal/general/model response publication is supported; NPC model targets
 * only. Earlier successful responses remain if a later backend call fails.
 * Pair flags and active-body gates retain original meaning; hits counts calls
 * returning nonzero and is preserved on error. Lists remain actor-only. */
int rf_scene_actor_pairs_process(rf_collision_pair_list *pairs,uint32_t *hits);
/*408f20 unholster using retained class delay, NPC timers, registered parent
 * health and current playback. Sound failure preserves prior playback effects;
 * unimplemented mapping/object families return errors. Does not schedule AI. */
int rf_scene_npc_recover_unholster(uint32_t handle,int32_t now_ms,
    int (*sound)(void *,uint32_t,const char *),void *context);
/* Retained NPC4091d0/409210 reset with actual action mapping, remaining-time
 * queries and shared nonlooping stop; stale handles reject before mutation. */
int rf_scene_npc_reset_ai_animation(uint32_t handle,uint32_t secondary);
/*503400 ->501cd0(kind2)->51c390 on the currently published model pose.
 * Zero exact non-looping weights without releasing references or removing
 * slots. Resolves actor or transferred-corpse ownership; no allocation. */
int rf_scene_model_stop_nonlooping(uint32_t model_slot);
/* Death CLEAR_BONE: clear only the current owner's override-enabled byte.
 * Nonnegative indices preserve playback/caches; UINT32_MAX models original
 * index-1 clearing the low byte of slot15 tick, without touching overrides. */
int rf_scene_model_clear_bone_override(uint32_t model_slot,uint32_t bone);

/* Death stage's428c90(actor,action,1,freeze,1) resource binding. Publish the
 * retained death action before loading/starting playback; later errors retain
 * preceding action/playback effects. Base unarmed mappings only; stale handles
 * and missing mappings do not start playback. Audio is caller-owned. */
int rf_scene_npc_death_play(uint32_t handle,int32_t action,uint32_t freeze,
    int (*sound)(void *,uint32_t,const char *),void *context);
/* Ordinary-SP NPC animation stage for the current base/unarmed class view.
 * Supplied selection and sound callbacks keep registrations/resources alive.
 * Includes reset, bone clears and playback; not complete death dispatch. */
typedef struct rf_scene_death_motion_ops {
    int (*select)(void *,uint32_t,int32_t *);
    int (*sound)(void *,uint32_t,const char *);
    void *context;
} rf_scene_death_motion_ops;
int rf_scene_npc_death_motion(uint32_t handle,const rf_scene_death_motion_ops *ops);
typedef struct rf_scene_death_selection_context {
    const rf_geometry_collision_world *world;
    rf_entity_death_obstacle *scratch;uint32_t capacity;rf_random_state *random;
} rf_scene_death_selection_context;
/* Compatible with death_motion_ops.select. Current base/unarmed pose state;
 * no allocations. Query failure preserves result/RNG, scratch may change. */
int rf_scene_npc_death_select(void *context,uint32_t handle,int32_t *action);
/* Compatible sound callback using the same selection context/RNG, original
 * action position (actor+3c), unity spatial playback and no stored voice. */
int rf_scene_npc_death_sound(void *context,uint32_t handle,const char *name);
extern uint32_t rf_scene_npc_action_audio[9];
/*49ce88..49cecf NPC impact sound: retained class128 group, original group
 * selection, on-demand PCM and unity spatial playback at bodye4. Caller
 * establishes lethal impact eligibility. RNG/audio effects are not rolled
 * back on playback failure; no voice is stored in the NPC. */
int rf_scene_npc_impact_sound(uint32_t handle,rf_random_state *random);
extern uint32_t rf_scene_npc_impact_audio[12];
extern uint32_t rf_scene_death_animation_test_enabled,rf_scene_death_animation_test[8];






/* Resolved local-player entity portion of40e0b0. Uses the same retained
 * camera-effect owner as force feedback and the rendered camera update.
 * Strength/duration replace the previous effect. A stale/nonlocal entity
 * returns NOT_FOUND without changing it. No health or HUD damage here. */
int rf_scene_player_feedback(uint32_t player_entity_handle,float strength,float duration,int32_t now);
/* Separate retained local-player flash owner. Damage replaces color/alpha
 * with original4a7520 red128. Draw/decay is called by the HUD/render owner,
 * not by camera shake or simulation ticks. The campaign pass uses the same owner.
 * Stale/nonlocal handles preserve outputs and return NOT_FOUND. */
int rf_scene_player_damage_flash(uint32_t player_entity_handle);
extern uint32_t rf_scene_player_vitals[6];
/* Registered local-player damage routing through4892c0/41a350. Owns vitals,
 * immunity, local-player predicates, red flash and original no-flinch behavior
 * for the campaign first-person eye profile. Other camera profiles delegate.
 * All other effects/predicates use the complete caller backend. Callbacks must preserve
 * owner lifetime and must not reenter damage. Missing/nonlocal target is zero.
 * This does not supply weapons, death, sound or authored hazard activation. */
int rf_scene_player_damage(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,const rf_damage_effect_backend *effects,float *result);
/* Also handles first-person player pain/death audio with the caller's shared
 * RNG and millisecond clock. Other reactions still use the complete backend.
 * Other profiles delegate; audio failure preserves committed damage. */
int rf_scene_player_damage_audio(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,int32_t now_ms,rf_random_state *random,const rf_damage_effect_backend *effects,float *result);
int rf_scene_player_flash_step(uint32_t player_entity_handle,float seconds,uint32_t freeze,
    rf_screen_flash *draw,uint32_t *active);
/* Registered skeletal NPC damage adapter. Effects must be synchronous and keep
 * owners alive; callbacks mutate the retained damage state, not stale copies.
 * Does not supply gameplay effects. Unknown/stale target is successful zero.
 * kind must be-1..10. Caller supplies real clock and difficulty settings. */
extern uint32_t rf_scene_death_clearance_test[8];
/* Read-only clearance for registered scene actors. Caller supplies capacity
 * for all registered entities; scratch may change on error, allowed does not.
 * Uses player published position/body orientation and stationary NPC pose.
 * Unknown owner families fail. Slot order is harmless for this read-only
 * any-blocker scan; this does not implement original factory list ordering. */
int rf_scene_death_clearance(const rf_geometry_collision_world *world,uint32_t handle,uint32_t direction,
    rf_entity_death_obstacle *scratch,uint32_t capacity,uint32_t *allowed);
int rf_scene_npc_damage(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,const rf_damage_effect_backend *effects,float *result);
extern uint32_t rf_scene_npc_damage_test_uid,rf_scene_npc_damage_test_words[64];
typedef struct rf_scene_npc_impact_services {
    const rf_damage_effect_backend *effects;rf_random_state *random;
    float difficulty;uint32_t clock_bits;
    /* Complete49cedf..49cf31 for the resolved local player's entity. */
    int (*player_feedback)(void *,uint32_t player_entity,float amount);
    void *context;
} rf_scene_npc_impact_services;
/*49cd80 SP scene binding: retained body/class/support, current force regions,
 * registered damage adapter and impact audio. Damage effects must preserve
 * actor lifetime and publish retained mutations synchronously. Linked-player
 * feedback requires its callback; unassociated NPCs have no such effect.
 * Errors stop after already-applied effects. Does not schedule collisions. */
int rf_scene_npc_impact(uint32_t handle,float speed,const rf_scene_npc_impact_services *services);
extern uint32_t rf_scene_npc_impact_dispatch[6]; /* calls,suppressed,damage,sound,player lookup,errors */
extern uint32_t rf_scene_npc_impact_test[4]; /* cases,health before/after,errors */

/* Replay-only first movement-region fixture: 1=center, 2=outside near base.
 * Does not establish ground clearance or replace the authored spawn. */
int rf_scene_stage_climb(rf_level *level,uint32_t mode);
/* Explicit L1S1 lower-door collision fixture, not an authored spawn. */
int rf_scene_stage_door(rf_level *level);
int rf_scene_stage_lift(rf_level *level);
int rf_scene_stage_force(rf_level *level,uint32_t uid);
/* Follow fixture: 1 MiB world projection plus 1 MiB actor output. */
#define RF_SCENE_FOLLOW_CAPACITY (3u*1024u*1024u)
/* Optional port-owned profiling clock in milliseconds; NULL disables. Counts
 * start at tick 16 to exclude startup work. No changes to simulation timing. */
void rf_scene_set_profile(uint32_t (*milliseconds)(void));
/* Rows: boundary, animation/stance, view/projection, view hash, model rendering,
 * scene checks/support, platform presentation/checks, physics commit.
 * Each row: calls, elapsed low/high ms, maximum ms. */
extern uint32_t rf_scene_profile[8][4],rf_scene_profile_stage[2];
/* Render-derived eligibility telemetry: summary frames/bytes/rooms/portals/
 * visible/start room; ring rows frame/start/visible/cached/eligibility hash,
 * followed by camera position and unscaled basis. Available to native probes. */
extern uint32_t rf_scene_visibility_summary[6],rf_scene_visibility_frames[64][17];
/* Particle summary: ticks, emitters, bytes, created, expired, live0, live1, RNG.
 * Ring: tick, first-created, stepped, expired, second-created, first/second
 * emitter calls, live0/live1, RNG, active-record hash, status. No draw calls yet. */
extern uint32_t rf_scene_particles_summary[8],rf_scene_particles_frames[64][12];
/* Inspection harness: camera at first authored emitter for ticks 0..399,
 * then normal camera, to exercise emission followed by expiry. Not player input. */
extern uint32_t rf_scene_particle_view_enabled;
/* Optional PC inspection: four units above/behind the source, looking down at it. */
extern uint32_t rf_scene_particle_view_back;
typedef int (*rf_scene_particle_sink)(void *context,const rf_particle_draw_vertex *vertices,
    uint32_t count,const rf_image *image,uint32_t mode);
/* Convert reciprocal world depth to the preview mesh's 24-bit Z convention. */
#define RF_SCENE_PARTICLE_DEPTH_BIAS ((1000.0f/999.9f)*16777215.0f)
#define RF_SCENE_PARTICLE_DEPTH_SCALE (-0.1f*RF_SCENE_PARTICLE_DEPTH_BIAS)
typedef void (*rf_scene_audio_sink)(void *context,const int16_t *stereo,uint32_t frames);
/* Optional synchronous platform sink. Borrows stereo48kHz PCM for this call
 * only; copy into bounded device storage. NULL keeps deterministic mixing only.
 * Configure before streaming; clear before destroying the sink context. */
void rf_scene_set_audio(rf_scene_audio_sink sink,void *context);
struct rf_wave_pcm;
struct rf_audio_mixer;
struct rf_audio_bank;
/* Optional diagnostic observer immediately before each scene mix block.
 * Borrowed state is read-only and valid only during the callback. Configure
 * outside streaming; NULL disables. No allocations or device operations here. */
typedef void (*rf_scene_audio_observer)(void *context,const struct rf_audio_mixer *mixer,
    const struct rf_audio_bank *bank,uint32_t frames);
void rf_scene_set_audio_observer(rf_scene_audio_observer observer,void *context);
typedef struct rf_scene_audio_events {
    void (*play)(void *context,uint32_t handle,const struct rf_wave_pcm *pcm,float left,float right);
    void (*stop)(void *context,uint32_t handle);
    void (*poll)(void *context);
    void (*reset)(void *context);
    void (*gain)(void *context,uint32_t handle,float left,float right);
    /* Explicit static-buffer loop mode; RF_OK only after device start succeeds.
     * Whole-buffer looping; PCM remains borrowed under the same reset contract. */
    int (*play_mode)(void *context,uint32_t handle,const struct rf_wave_pcm *pcm,float left,float right,uint32_t looping);
    /* RF_OK certifies no device borrowers remain for this PCM base. */
    int (*release_idle_sample)(void *context,const uint8_t *samples);
    /* Nonzero while the matched device source is running, zero for unknown,
     * completed, released or closed voices. Uses the device clock, not scene
     * replay time; muted running sources still count. Does not certify PCM
     * release or that already queued output has reached the speakers. */
    uint32_t (*playing)(void *context,uint32_t handle);
} rf_scene_audio_events;
/* Device event adapter: PCM is borrowed until reset, which MUST synchronously
 * release all device references before returning. Events use logical mixer
 * handles; devices maintain their own playback clock. Configure before stream. */
void rf_scene_set_audio_events(const rf_scene_audio_events *events,void *context);
/* Synchronous preview pass, valid only inside the scene frame sink. Uses the
 * current camera and retained particle textures. NULL sink checks packets.
 * World/actor mesh is presented first by this diagnostic composition; complete
 * mixed-object/room-surface integration remains separate. No per-frame allocation. */
int rf_scene_draw_particles(rf_scene_particle_sink sink,void *context);
int rf_scene_draw_coronas(rf_scene_particle_sink sink,void *context);
/* Campaign HUD pass after world/particles, inside the frame sink only.
 * NULL sink checks the packet; otherwise callback must preserve player owners.
 * Emits an untextured 640x480 rectangle and commits fade after successful draw.
 * Uses the diagnostic frame step; no full game pause/HUD scheduling claim. */
int rf_scene_draw_player_flash(rf_scene_particle_sink sink,void *context);
/* Frames, queued entries, particles visited, polygons, vertices, cumulative packet
 * hash, allocated workspace bytes. Ring rows: frame/queued/visited/polygons/vertices/hash. */
extern uint32_t rf_scene_particle_draw_summary[7],rf_scene_particle_draw_frames[64][6];
typedef struct rf_scene_world_geometry {
    const rf_geometry *world;
    rf_geometry_movers movers;
    uint32_t *offsets,*slots;
    uint32_t geometry_count,material_count,allocated_bytes;
    float camera_position[3],camera_orientation[3][3];
} rf_scene_world_geometry;
/* Retain mover source geometry and local material mappings for reprojection.
 * Borrows world (must outlive this owner); copies only the preview camera.
 * Archives and the source level may close after success. Output mesh, material
 * images, and geometry owner have separate ownership and close functions.
 * All outputs must be empty; failure preserves them. Geometry payload cap is
 * 1 MiB, plus owner and temporary geometry pointer array; mapping allocations
 * are included in material_budget during load and allocated_bytes afterward. */
int rf_scene_world_open_retained(const rf_level *level,const rf_geometry *world,
    rf_vpp *maps,uint32_t map_count,rf_preview_mesh *mesh,rf_materials *materials,
    uint32_t mesh_budget,uint32_t material_budget,rf_scene_world_geometry *geometry);
void rf_scene_world_geometry_close(rf_scene_world_geometry *geometry);
/* No allocation or archive access. Poses must match retained mover order and
 * count; NULL with zero pose_count selects authored file poses. Keeps the saved
 * inspection camera. Same stable-input/capacity contract as preview updater. */
int rf_scene_world_update(const rf_scene_world_geometry *geometry,
    const rf_group_attached_pose *poses,uint32_t pose_count,
    rf_preview_mesh *mesh,uint32_t capacity_bytes);
/* Reproject retained geometry using an explicit camera without mutating its
 * saved inspection view. No allocation or archive access. Finite camera inputs
 * are required; invalid camera input leaves mesh unchanged. Remaining failure
 * semantics are those of rf_scene_world_update/preview updater. */
int rf_scene_world_update_camera(const rf_scene_world_geometry *geometry,
    const rf_group_attached_pose *poses,uint32_t pose_count,const float position[3],
    const float orientation[3][3],rf_preview_mesh *mesh,uint32_t capacity_bytes);
/* Same camera update, with disjoint caller-owned staging storage. Destination
 * is preserved on failure; scratch may change. See preview staging contract. */
int rf_scene_world_update_camera_staged(const rf_scene_world_geometry *geometry,
    const rf_group_attached_pose *poses,uint32_t pose_count,const float position[3],
    const float orientation[3][3],rf_preview_mesh *mesh,uint32_t capacity_bytes,
    rf_preview_vertex *scratch,uint32_t scratch_bytes);
/* Load authored mover meshes with the world and a deduplicated texture table.
 * Outputs must be empty. Geometry source budget is 1 MiB plus pointer array;
 * mesh/material budgets include their own temporary allocations. Source mover
 * geometry is released after projection; runtime motion is not yet connected. */
int rf_scene_world_open(const rf_level *level,const rf_geometry *world,
    rf_vpp *maps,uint32_t map_count,rf_preview_mesh *mesh,rf_materials *materials,
    uint32_t mesh_budget,uint32_t material_budget);
/* Diagnostic close inspection camera 2.2 units in front of an authored actor.
 * Changes only the supplied level's preview camera; not a gameplay camera. */
int rf_scene_preview_camera(rf_level *level,int32_t uid);
/* Fixed diagnostic view of the positive-X route endpoint; not a player camera. */
int rf_scene_preview_route_camera(rf_level *level,int32_t uid);
/* Inspection only: view a mover along its thinnest local axis, centered on
 * vertex bounds with world-up. Positive distance selects one side, negative
 * the other. Does not recover gameplay camera/collision placement. */
int rf_scene_preview_mover_camera(rf_level *level,int32_t uid,float distance);
/* Append one authored miner using scripted frame 0 to an existing world mesh.
 * Port-owned diagnostic composition, not a scene/gameplay loader. On success
 * mesh/materials own the combined arrays/images; on failure remain unchanged.
 * Budgets cap final mesh bytes and material residency; temporary old/new arrays
 * coexist during commit, plus a 1 MiB actor mesh and animation workspace.
 * Caller records the original mesh count as the actor draw-range boundary. */
int rf_scene_preview_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget);
/* Stream 64 scripted poses with fixed world prefix/materials and one combined
 * allocation (world bytes plus 1 MiB actor capacity, included in mesh_budget).
 * Sink borrows the current combined mesh synchronously; it must not mutate it.
 * Setup failure preserves inputs. Once streaming starts, inputs own combined
 * resources even on sink/producer failure; caller closes them on every exit.
 * Last produced pose remains in mesh. Textures are loaded only during setup. */
typedef int (*rf_scene_frame_sink)(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world_vertices);
int rf_scene_stream_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context);
/* Same ownership/budgets as stream_miner; loads the actor class's base state set
 * and runs the authored-state inspection schedule. Additional state-set storage
 * and a 512 KiB temporary registration budget are outside mesh/material caps. */
int rf_scene_stream_miner_states(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context);
/* Integrated diagnostic: sweep retained actor spheres two units along each
 * world axis against stationary geometry, without moving the body. Query mask
 * 0x460 is an inspection choice, not recovered gameplay movement policy. */
int rf_scene_actor_world_check(const rf_geometry_collision_world *world,uint32_t out[8]);
/* Passive falling fixture on a copy of the actor state; completes the first
 * collision frame with bounded remaining-time passes, then stops. Rendered
 * body remains untouched. 120 frames maximum before the first contact. */
int rf_scene_actor_fall_check(const rf_geometry_collision_world *world,uint32_t out[8]);
/* Per-frame physics diagnostic with bounded collision passes. Borrows the
 * resident stationary world and its matching source geometry for surface names.
 * Scripted animation/spawn assumptions remain; AI and full entity lifecycle
 * are not implemented. Caller retains both geometry objects throughout. */
int rf_scene_stream_miner_body(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context,const rf_geometry_collision_world *collision,const rf_geometry *geometry);
/* Diagnostic process-local input; no host input or gameplay controller.
 * Profile 0: passive; 1: +X .25 frames 24..47; 2: -X 1 frames 24..62. */
void rf_scene_actor_drive(int profile);
/* Scene-owned initial world/mover alpha query. UINT32_MAX selects world;
 * mover IDs are retained indices. Contacts remain solid-local; face is an
 * authored source index. No runtime texture overrides yet.
 * Serialized UV/tree scratch, no query allocation; errors preserve output. */
int rf_scene_geometry_texture_query(uint32_t solid,uint32_t flags,const float start[3],const float delta[3],
    float radius,float limit,rf_geometry_world_sweep_hit *result,uint32_t *matched);
/* Cached hit must belong to this solid and the current retained scene lifetime.
 * Resolve its authored face/room; flag1 enables preferred-first testing.
 * Invalid cached references fail with output preserved; NULL uses full traversal.
 * World resolution scans only the cached room, without allocating storage. */
int rf_scene_geometry_texture_query_preferred(uint32_t solid,const rf_geometry_world_sweep_hit *cached,
    uint32_t flags,const float start[3],const float delta[3],float radius,float limit,
    rf_geometry_world_sweep_hit *result,uint32_t *matched);
extern uint32_t rf_scene_geometry_textures[13];
/* Query-only alpha callback counts: total, transparent, opaque, errors. */
extern uint32_t rf_scene_geometry_alpha_contacts[4];
/* Glare backend solid callback: token1 world, token2+i mover. Reset1 and
 * solid-local flag4 required. Face tokens span all current scene bindings;
 * a preferred face may belong to another solid. Tokens expire at scene close.
 * Normalized reset output is zero on miss, untouched on error. No allocation. */
int rf_scene_glare_solid_query(void *context,uint32_t solid,const rf_glare_solid_query *query,
    rf_collision_solid_response_hit *out,uint32_t reset);
/* Calls, cached calls, hits, query errors, normalized-result hash. */
extern uint32_t rf_scene_glare_solids[5];
/* Current registered NPC visibility projection; authored orientation remains
 * the existing NPC pose convention. Model identity is borrowed for scene lifetime.
 * No room membership, actor-list ordering or selected-player policy supplied. */
int rf_scene_npc_visibility_view(uint32_t handle,rf_glare_visibility_object *result);
int rf_scene_npc_visibility_model(void *context,const rf_collision_visibility_object *object,
    rf_collision_model_part_query *query,rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted);
extern uint32_t rf_scene_npc_visibility[7];
/* Registered actor's retained room token (word0), linked class1 predicate,
 * and linked actor handle (UINT32_MAX when absent). Initial room binding uses
 * authored position; post-update refresh maintains it. Errors preserve output. */
int rf_scene_npc_visibility_facts(uint32_t handle,uint32_t result[3]);
extern uint32_t rf_scene_npc_visibility_rooms[6];
/*48a190 stage for a registered NPC; uses its published position and existing
 * retained world locator. Synchronizes flags and still-owned model room.
 * Local-player room notifications and post-refresh family effects excluded. */
int rf_scene_npc_refresh_room(uint32_t handle);
extern uint32_t rf_scene_npc_room_refresh[8];
/* Frames, dispatches, completed families, flag skips, owner/flag hash, errors. */
extern uint32_t rf_scene_npc_render_dispatch[6];
/* Borrow a retained world for diagnostic camera following; NULL disables.
 * Owner and source world must outlive the body stream. Fixed .7Y/2.4Z offset,
 * no camera collision or original first-person policy. */
void rf_scene_actor_follow(const rf_scene_world_geometry *world);
/* First-person diagnostic from the moving body's cached eye and controller.
 * Requires retained follow world; no player input, look rotation or camera collision. */
extern uint32_t rf_scene_actor_eye_enabled;
/* Process-local pitch-only look profile on the retained route. */
extern uint32_t rf_scene_actor_look_enabled,rf_scene_actor_turn_enabled;
/* Staged README scene: original geometry, miner and half-open authored doors.
 * Diagnostic placement only; no campaign trigger or NPC behavior claim. */
extern uint32_t rf_scene_showcase_enabled;
int rf_scene_showcase_camera(rf_level *level);
typedef struct rf_scene_input {float move[3],look[2];uint32_t crouch,jump,use;} rf_scene_input;
/* Poll once before stance/animation/physics. RF_NOT_FOUND ends the stream cleanly.
 * Finite axes in [-1,1], crouch/jump/use held states 0/1. Caller owns context until stream ends.
 * Zero frame_limit permits a UINT32_MAX-frame session with bounded rings. */
/* RFI3 + uint32 size32 includes use; RFI2 size28 omits use; legacy raw
 * size24 omits jump/use. Readers must zero omitted fields before loading. */
int rf_scene_replay_header(FILE *file,uint32_t *count,uint32_t *record_size);
extern uint32_t rf_scene_player_jump[4],rf_scene_player_jump_frames[128][8];
typedef int (*rf_scene_input_poll)(void *context,uint32_t frame,rf_scene_input *input);
void rf_scene_set_input(rf_scene_input_poll poll,void *context,uint32_t frame_limit);
/* Campaign-start diagnostic: copy the level start and load miner1 by class.
 * NULL disables. This connects placement/look only; complete player factory,
 * weapon selection, class cache ownership and campaign events remain separate. */
int rf_scene_set_campaign_spawn(const rf_level *level);
extern uint32_t rf_scene_player_spawn_diagnostic[19];
#endif
