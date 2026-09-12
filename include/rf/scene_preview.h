#ifndef RF_SCENE_PREVIEW_H
#define RF_SCENE_PREVIEW_H
#include "rf/material.h"
#include "rf/preview.h"
#include "rf/entity.h"
#include "rf/random.h"
#include "rf/event.h"
#include "rf/eye.h"
#include "rf/player.h"

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
/*503400 ->501cd0(kind2)->51c390 on the currently published model pose.
 * Zero exact non-looping weights without releasing references or removing
 * slots. Resolves actor or transferred-corpse ownership; no allocation. */
int rf_scene_model_stop_nonlooping(uint32_t model_slot);
/* Death CLEAR_BONE: clear only the current owner's override-enabled byte.
 * Preserve basis, weight, playback and cached matrices/generation stamps. */
int rf_scene_model_clear_bone_override(uint32_t model_slot,uint32_t bone);

/* Death stage's428c90(actor,action,1,freeze,1) resource binding. Publish the
 * retained death action before loading/starting playback; later errors retain
 * preceding action/playback effects. Base unarmed mappings only; stale handles
 * and missing mappings do not start playback. Audio is caller-owned. */
int rf_scene_npc_death_play(uint32_t handle,int32_t action,uint32_t freeze,
    int (*sound)(void *,uint32_t,const char *),void *context);



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
