#ifndef RF_WEAPON_H
#define RF_WEAPON_H
#include "rf/motion.h"
#include "rf/entity.h"
#include "rf/collision.h"
typedef struct rf_weapon_flight {
    float position[3],velocity[3],radius;
    double remaining;
    uint32_t active;
} rf_weapon_flight;
typedef struct rf_weapon_flight_contact {
    rf_collision_ray_hit hit;
    uint32_t object,room,face;
} rf_weapon_flight_contact;
typedef struct rf_weapon_flight_event {
    uint32_t kind; /*0 flying/inactive,1 impact,2 expired. */
    rf_weapon_flight_contact contact;
} rf_weapon_flight_event;
typedef int (*rf_weapon_flight_sweep)(void *,const float start[3],const float delta[3],
    float radius,rf_weapon_flight_contact *,uint32_t *matched);
/* Practical straight-flight first pass, not original homing/gravity/fuse logic.
 * Zero-initialize before first launch. Launch normalizes direction and refuses
 * to overwrite an active projectile.
 * Step sweeps the whole path (clamped to lifetime), then advances its center
 * to the hit fraction; event contact remains the surface hit, not the center.
 * Impact wins a tie at expiry. Terminal events occur once. No allocation.
 * Callback/input errors preserve projectile and event; callback scratch may
 * change. Callback must not mutate/alias the projectile or event. */
int rf_weapon_flight_launch(rf_weapon_flight *,const float position[3],const float direction[3],
    float speed,float lifetime,float radius);
int rf_weapon_flight_step(rf_weapon_flight *,float dt,rf_weapon_flight_sweep,void *,rf_weapon_flight_event *);

/* First-pass uniform solid-angle spread using an explicit deterministic stream.
 * Preserves ray length; zero spread preserves the ray and does not draw RNG.
 * Invalid input preserves output/state. Not a retail sampling-order claim. */
int rf_weapon_spread_ray(const float ray[3],float degrees,rf_random_state *random,float result[3]);

/* First-pass fixed-tick firing policy. Accepted bursts finish after trigger release;
 * blocked (reload/death/switch) cancels queued shots and consumes trigger edges.
 * Caller consumes one inventory round for event1; event2 requests dry/reload handling.
 * No allocation, ammo mutation or RNG. Invalid inputs preserve state/output. */
typedef struct rf_weapon_trigger_rules {uint32_t fire_ticks,burst_count,burst_ticks,semi_automatic;} rf_weapon_trigger_rules;
typedef struct rf_weapon_trigger_state {uint32_t cooldown,remaining,delay,held;} rf_weapon_trigger_state;
int rf_weapon_trigger_step(rf_weapon_trigger_state *,const rf_weapon_trigger_rules *,
    uint32_t trigger,uint32_t blocked,uint32_t loaded,uint32_t *event);

/* Port-owned fixed60Hz continuous charge drain. Preserve the remainder across
 * release/repress so tapping cannot create free charge. Reset on a new battery.
 * Exactly capacity units drain over drain_ticks active ticks; output/state and
 * loaded charge remain unchanged on invalid input. */
int rf_weapon_charge_step(uint32_t *remainder,uint32_t capacity,uint32_t drain_ticks,
    uint32_t active,int32_t *loaded,uint32_t *consumed);

#define RF_PROJECTILE_CAPACITY 50u
#define RF_PROJECTILE_RECORD_WORDS 197u
/* Original48b4d0/48b590/48b610 fixed storage; raw record fields are populated
 * by the pending object factory. Free links use slot+1 tokens instead of host
 * pointers. No heap fallback on stock Xbox. Never reset a pool with live owners. */
typedef struct rf_projectile_pool {
    uint32_t records[RF_PROJECTILE_CAPACITY][RF_PROJECTILE_RECORD_WORDS];
    uint32_t head,free_count,live_count,peak,live_bits[2];
} rf_projectile_pool;
void rf_projectile_pool_init(rf_projectile_pool *);
/* Exhaustion returns NOT_FOUND without modifying slot. Acquired payload is
 * retained, not zeroed. Release overwrites only its free-link word. Registry,
 * model, effects and physics must be retired separately before release. */
int rf_projectile_pool_acquire(rf_projectile_pool *,uint32_t *slot);
int rf_projectile_pool_release(rf_projectile_pool *,uint32_t slot);

typedef struct rf_projectile_owner {
    uint32_t object_kind,handle,uid,slot,flags;rf_object_link object_link;
} rf_projectile_owner;
typedef struct rf_projectile_store {rf_projectile_pool pool;rf_projectile_owner owners[50];} rf_projectile_store;
struct rf_projectile_creation_descriptor;
typedef struct rf_projectile_owner_ops {
    int (*initialize)(void *,rf_projectile_owner *,uint32_t record[197],const struct rf_projectile_creation_descriptor *);
    void (*cleanup)(void *,rf_projectile_owner *,uint32_t record[197]);
} rf_projectile_owner_ops;
/* Port owner adapter for the verified fixed pool, generic object list and
 * handle registry. Fresh stores only; no heap allocation. Init/cleanup borrow
 * all owners; no reentry or registry/list mutation in callbacks. Cleanup must
 * release partial resources and cannot fail. Marking flags bit2 does not close. */
void rf_projectile_store_init(rf_projectile_store *);
int rf_projectile_store_open(rf_projectile_store *,rf_object_registry *,rf_object_list *,uint32_t *uid_cursor,
    const struct rf_projectile_creation_descriptor *,const rf_projectile_owner_ops *,void *,rf_projectile_owner **);
/* Detach list before cleanup, retain registry identity during cleanup, then
 * release handle and pool slot. Stale handles do not close reused slots. */
int rf_projectile_store_close(rf_projectile_store *,rf_object_registry *,rf_object_list *,uint32_t handle,
    const rf_projectile_owner_ops *,void *);

typedef struct rf_projectile_descriptor_input {
    uint32_t name_length,name_token,model_token,flags_264,flags_268;
    float field_bc,field_ac,speed,speed_scale;
    uint32_t multiplayer,player_controlled,powerup;
    int32_t weapon,weapon_count,special_weapon;float position[3],basis[9];
} rf_projectile_descriptor_input;
typedef struct rf_projectile_creation_descriptor {uint32_t words[38];uint32_t boosted;} rf_projectile_creation_descriptor;
/*4c77a0 prefix through486da0: zeroed152-byte descriptor plus boost decision.
 * Tokens are caller-owned32-bit references; predicate values use original low
 * bytes. Does not allocate objects or resources. Finite geometry required;
 * invalid factory weapon range returns NOT_FOUND without output mutation. */
int rf_projectile_descriptor_prepare(const rf_projectile_descriptor_input *,rf_projectile_creation_descriptor *);

typedef struct rf_weapon_world_model {
    uint32_t name_nonempty,model;int32_t muzzle,grip;
} rf_weapon_world_model;
/*4c84d0: absent name or out-of-range weapon yields no model. Loaded tokens
 * belong to the model owner; this accessor neither loads nor draws. */
uint32_t rf_weapon_world_model_token(const rf_weapon_world_model models[64],int32_t weapon);
/*4c8510/4c8560: kind0=grip_1,1=muzzle_1. A cached-1 is looked up again;
 * other signed values are retained. Missing model returns tag-1. Callback
 * errors preserve cache/output; callback must retain the descriptor array. */
int rf_weapon_world_tag(rf_weapon_world_model models[64],int32_t weapon,uint32_t kind,
    int (*lookup)(void *,uint32_t,const char *,int32_t *),void *context,int32_t *tag);

/*421d85..421df6: rotate about the pose third basis vector, without axis
 * normalization. Hand0 negates recoil, hand1 keeps its sign. Finite inputs;
 * errors preserve output, alias allowed. Position is unchanged by this stage. */
int rf_weapon_recoil_basis(const float basis[9],float recoil,int32_t hand,float result[9]);

/*421df6..421e4f final80-byte held-weapon draw state. Preserve words3/6
 * except special-view low byte1 replaces word3. Tint is actor1474; basis is
 * the post-recoil pose. No geometry submission. Input basis may alias state. */
int rf_weapon_draw_state_prepare(uint32_t state[20],uint32_t special_view,
    uint32_t tint,const float basis[9]);

typedef struct rf_weapon_world_view {
    uint32_t flags_810,class_flags_724,inventory_flags_7d0;
    int32_t weapon,attachment_75c,linked_kind;
    uint32_t player_present,player_1044,player_103c;
    int32_t special_weapon;uint32_t override_model;
} rf_weapon_world_view;
/*421c40..421d47: clear810 bit200, visibility gates, select loaded model.
 * linked_kind is the stable resolved486c90 class (or0 for absent).
 * Player byte fields use their low byte. No draw/hand loop or final bit200 set. */
int rf_weapon_world_visibility(rf_weapon_world_view *view,
    const rf_weapon_world_model models[64],uint32_t *model);

typedef struct rf_weapon_hand_source {
    uint32_t actor_model;int32_t weapon;uint32_t hand_count;int32_t hands[2];
    float position[3],basis[9];
} rf_weapon_hand_source;
typedef struct rf_weapon_hand_placement {float hand[3],position[3],basis[9];} rf_weapon_hand_placement;
typedef struct rf_weapon_hand_ops {
    int (*tag)(void *,uint32_t,const char *,int32_t *);
    int (*transform)(void *,uint32_t,int32_t,const float basis[9],const float position[3],float out_basis[9],float out_position[3]);
} rf_weapon_hand_ops;
/*418e60: hand transform, copied output pose, then grip offset correction.
 * Missing hand/model returns NOT_FOUND without output writes. Callbacks retain
 * source/models and may change source.weapon; grip lookup rereads that weapon,
 * while the second transform uses the initially captured weapon model. Errors
 * preserve earlier output writes. Successful transforms must write the complete
 * output pose. Full model transform is a supplied service. */
int rf_weapon_place_in_hand(const rf_weapon_hand_source *source,int32_t hand,
    rf_weapon_world_model models[64],const rf_weapon_hand_ops *ops,void *context,rf_weapon_hand_placement *result);

typedef struct rf_weapon_aim_source {
    uint32_t local_related,animation_locked,target_present,target_actor;
    float target_position[3],target_eye[3],eye_basis[9];
} rf_weapon_aim_source;
/*41b4c0 after resolved48aaf0/428700 and target lookups. Locked animations
 * preserve incoming basis; local-related low byte1 or absent target uses eye
 * basis. Targets outside forward dot>0.8 preserve incoming basis. Does not
 * perform predicate side effects or registry lookup. Inputs finite; no alias. */
int rf_weapon_target_aim(const rf_weapon_aim_source *,const float muzzle[3],float basis[9]);

typedef struct rf_weapon_muzzle_source {
    uint32_t actor_model;int32_t weapon,primary_limit,primary_index,secondary_index;
    uint32_t primary_count;int32_t primary_tags[2],secondary_tags[2];
    float position[3],basis[9],eye[3],eye_basis[9];
} rf_weapon_muzzle_source;
/*41b5a0: primary count gates both weapon families; secondary tag lookup uses
 * the separate original actor294 owner. Inputs remain stable across callbacks.
 * Muzzle origin uses the hand pose directly (no grip correction). Aim is the
 * explicit41b4c0 service, reached only after both transforms. Missing model/tag
 * or primary hands uses eye + 0.3f*eye forward. Errors preserve completed work. */
int rf_weapon_muzzle_pose(const rf_weapon_muzzle_source *,rf_weapon_world_model models[64],
    const rf_weapon_hand_ops *,int (*aim)(void *,const float position[3],float basis[9]),
    void *,float position[3],float basis[9]);

typedef struct rf_weapon_world_draw {
    rf_weapon_world_view view;uint32_t hand_count;float recoil;uint32_t special_view,tint;
} rf_weapon_world_draw;
typedef struct rf_weapon_world_draw_ops {
    int (*place)(void *,int32_t,rf_weapon_hand_placement *);
    int (*submit)(void *,uint32_t,const rf_weapon_hand_placement *,const uint32_t state[20]);
} rf_weapon_world_draw_ops;
/*421c40 orchestration with explicit placement/submission boundaries. Scratch
 * supplies constructor-preserved bytes. Callback state must remain alive;
 * count/recoil/view mode/tint are reread, selected model stays captured.
 * Missing placement skips a hand. Errors retain completed calls and flag clear.
 * Successful visible-model path sets810 bit200 even with no submitted hands. */
int rf_weapon_world_draw_run(rf_weapon_world_draw *draw,const rf_weapon_world_model models[64],
    uint32_t scratch[20],const rf_weapon_world_draw_ops *ops,void *context);

typedef struct rf_weapon_presentation_state {
    uint32_t model,auxiliary; /* Player +34/+38; opaque 32-bit model tokens. */
    int32_t current,pending,deadline; /* +1080/+f80/+f84 (not queue timer +b8). */
} rf_weapon_presentation_state;
typedef struct rf_weapon_model_descriptor {
    uint32_t name_nonempty,model; /* Resolved +40 string and loaded +48 model. */
    int32_t resource; /* +64 argument to 50ce00. */
} rf_weapon_model_descriptor;
typedef struct rf_weapon_model_cache { int32_t weapon; uint32_t normal,alternate; } rf_weapon_model_cache;
typedef struct rf_weapon_presentation_context {
    uint32_t local_player;
    int32_t paired_first,paired_second,alternate_weapon,base_weapon;
    uint32_t mode; int32_t mode_weapon;
    uint32_t resource_backend,mode_kind; /* Globals 17c7bcc and 7cabbc. */
} rf_weapon_presentation_context;
typedef struct rf_weapon_presentation_ops {
    int (*resource)(void *user,int32_t resource); /* 550820, backend 0x66 only. */
    int (*mode_finish)(void *user); /* 48ab90 binding from 4b0610, mode_kind==1. */
} rf_weapon_presentation_ops;
/* 4ae0d0 over already-loaded model tokens and 32 cache entries. Missing
 * models return RF_NOT_FOUND at the original fatal assertion boundary.
 * Missing reached adapters preserve preceding mutations and leave result
 * unchanged. Callbacks may update state/context through user. No model load
 * or rendering is implied by installing an opaque model token. */
int rf_weapon_update_presentation(rf_weapon_presentation_state *state,int32_t weapon,
    const rf_weapon_model_descriptor descriptors[64],const rf_weapon_model_cache cache[32],
    const rf_weapon_presentation_context *context,const rf_weapon_presentation_ops *ops,
    void *user,uint32_t *result);

/* 4a5910 with 4ae0d0's nonlocal early return. Resolves linked classes 1/4
 * once from stable views, otherwise uses the primary entity's weapon[0].
 * Missing entities yield -1. For a local player, every value except exactly
 * -1 requires the presentation adapter before returning (including -2).
 * Adapter errors preserve output; successful model effects are caller-owned. */
int rf_weapon_current(const rf_entity_registry *registry,int32_t entity_handle,
    uint32_t local_player,int (*update_presentation)(void *user,int32_t weapon),
    void *user,int32_t *weapon);

typedef struct rf_weapon_selection_state {
    int32_t pending_weapon; /* Player +f80. */
    int32_t deadline; /* Player +b8. */
    uint8_t flag_f94,flag_f95,reserved[2];
    int32_t value_f98;
} rf_weapon_selection_state;
/* 4acd50 with its unchanged 4fa3e0 timer tail: queue the exact signed weapon
 * value and clear the deadline. The caller owns selection eligibility; this
 * primitive does not activate a weapon or load its presentation. */
int rf_weapon_queue_selection(rf_weapon_selection_state *state,int32_t weapon);
/* 4ad8a0: clears +f94/+f95/+f98; preserves the two intervening bytes. */
int rf_weapon_clear_followup(rf_weapon_selection_state *state);

typedef struct rf_weapon_selection_input {
    int32_t requested,paired_first,paired_second,category_split,current_primary,current_secondary;
    uint32_t paired_mask,player_flags,force_flag,defer_flag;
} rf_weapon_selection_input;
typedef struct rf_weapon_selection_ops {
    int (*already_selected)(void *user,int32_t weapon); /* 4a4cf5 formatted message. */
    int (*apply_queued)(void *user); /* 4aa0b0; not reconstructed yet. */
} rf_weapon_selection_ops;
/* 4a4c91..4a4db4, after earlier selection gates. Masks are entity +1428 and
 * player +10; force/defer are caller arguments. State +f94 is read after apply,
 * and nonzero invokes the shared followup clear. Callbacks may update state.
 * Missing reached adapters return RF_NOT_FOUND, retaining a queued request.
 * This is the selection tail, not the complete 4a4a50 operation. */
int rf_weapon_finish_selection(rf_weapon_selection_state *state,
    const rf_weapon_selection_input *input,const uint8_t owned[64],
    const uint32_t flags_264[64],uint32_t weapon_count,
    const rf_weapon_selection_ops *ops,void *user);

typedef struct rf_weapon_inventory {
    uint8_t owned[64]; /* Entity +42c. */
    int32_t reserve[32],loaded[64]; /* +2ac and +32c. */
} rf_weapon_inventory;
typedef struct rf_weapon_acquire_definition {
    int32_t ammo_type,capacity,magazine;
} rf_weapon_acquire_definition;
/*4257c0: consume one shot from loaded ammo when weapon<count and magazine>0,
 * otherwise from mapped reserve. Invalid weapon IDs are no-ops. Decrement
 * wraps to signed32 then clamps negative values to zero. No firing eligibility
 * or owned check; invalid reached ammo mappings fail without mutation. */
int rf_weapon_consume_shot(rf_weapon_inventory *,const rf_weapon_acquire_definition definitions[64],
    uint32_t weapon_count,int32_t weapon);

/*4030d0: initialize a previously unowned weapon, then call401470(inventory,1).
 * Quantity -1 fills a positive magazine without touching reserve. Arithmetic
 * wraps at32 bits before signed clamping. Notification errors retain changes.
 * Definition is the selected weapon descriptor; borrowed owners stay alive. */
int rf_weapon_acquire(rf_weapon_inventory *,const rf_weapon_acquire_definition *,
    int32_t weapon,int32_t quantity,int (*notify)(void *,rf_weapon_inventory *,uint32_t),
    void *context);

/* Normal SP (original64ecb9=0 and6fc4d8=0):401470 notification exits through
 * actual42cce0 before selection or RNG. This does not equip/reset a weapon. */
int rf_weapon_acquire_sp(rf_weapon_inventory *,const rf_weapon_acquire_definition *,
    int32_t weapon,int32_t quantity);

typedef struct rf_weapon_startup_state {int32_t primary,secondary;} rf_weapon_startup_state;
/*422cf5..422dbd after inventory construction, normal SP. Grant primary,
 * publish it, equip, reread class primary and fill its reserve; then secondary
 * grant/publication/refill and extra grant. Borrowed defaults/definitions may
 * change during equip. Errors preserve completed grants and publications. */
int rf_weapon_startup_grant_sp(rf_weapon_inventory *,rf_weapon_startup_state *,
    const int32_t defaults[3],const rf_weapon_acquire_definition definitions[64],
    int (*equip)(void *,int32_t),void *context);

typedef struct rf_weapon_pickup_grant {uint32_t rounds,acquired;} rf_weapon_pickup_grant;
/* First-pass nonnegative ammo grant. New weapon fills magazine then reserve;
 * existing weapon/ammo-only item adds capped reserve. Reject invalid state
 * without mutation. Output distinguishes new ownership from added rounds. */
int rf_weapon_pickup_grant_sp(rf_weapon_inventory *,const rf_weapon_acquire_definition *,int32_t weapon,int32_t quantity,uint32_t gives_weapon,rf_weapon_pickup_grant *);

/* First-pass reload completion: transfer min(missing magazine,reserve) rounds.
 * Owned magazine weapon only; rejects invalid/negative state without mutation.
 * Scheduling/cancellation are caller-owned; no allocation or notifications. */
int rf_weapon_reload_transfer(rf_weapon_inventory *,const rf_weapon_acquire_definition *,int32_t weapon,uint32_t *transferred);

typedef struct rf_weapon_ammo_state {int32_t current,pending,weapon_count;} rf_weapon_ammo_state;
typedef struct rf_weapon_ammo_backend {
    void *context;
    int (*is_reloading)(void *,uint32_t *);
    int (*reload)(void *,uint32_t,uint32_t);
} rf_weapon_ammo_backend;
/*428d90: negative reserve is zeroed before wrapping addition; pending reload
 * quantity receives the full addition before reserve is capped. Query425250
 * and reload425280(actor,0,0) are services. Errors retain preceding changes. */
int rf_weapon_add_ammo(rf_weapon_inventory *,rf_weapon_ammo_state *,
    const rf_weapon_acquire_definition *,int32_t weapon,int32_t quantity,
    const rf_weapon_ammo_backend *);

/*45a5a0..45a69c SP quantity math after pickup acceptance. Special ammunition
 * uses units of100; scale-disabled skips difficulty scaling only. Exact x87
 * 64-bit-significand rounding is reproduced using bounded integer arithmetic. */
int rf_weapon_pickup_amount(int32_t quantity,int32_t reserve,int32_t capacity,
    uint32_t difficulty,uint32_t special,uint32_t scale_disabled,
    int32_t *granted,int32_t *displayed);

typedef struct rf_weapon_supply {
    int32_t ammo_type,capacity; /* Descriptor +24 and +260. */
    uint32_t flags_268;
} rf_weapon_supply;
/* 42add0: null inventory, negative weapon or negative ammo type has zero
 * reserve. Added bounds checks reject invalid nonnegative indices. */
int rf_weapon_reserve(const rf_weapon_inventory *inventory,const rf_weapon_supply supply[64],
    int32_t weapon,int32_t *amount);
/* 4a6e50 after entity lookup: scans exactly 32 preference entries. Owned
 * weapons with capacity>0 require positive reserve+loaded (original 32-bit
 * wrapping addition). With low byte defer_flag nonzero, +268 mask 0x100
 * weapons are retained only as the first fallback. Null inventory selects -1.
 * Inputs remain unchanged; selected is preserved on a bounds error. */
int rf_weapon_choose_available(const rf_weapon_inventory *inventory,const rf_weapon_supply supply[64],
    const int32_t preference[32],uint32_t defer_flag,int32_t *selected);

/*4a70e0: release each nonzero player +10e8 slot through4cc010, then clear
 * that slot after the callback. Later slots are read when reached. Resource
 * tokens and their pool lifetime belong to the release backend. */
int rf_weapon_release_player_slots(uint32_t slots[25],void (*release)(void *,uint32_t),void *context);

typedef struct rf_weapon_remove_backend {
    const uint32_t *players;const int32_t *count,*special_weapon;uint32_t capacity;
    rf_weapon_inventory *(*inventory)(void *,uint32_t player);
    void (*notify)(void *,uint32_t player);void *context;
} rf_weapon_remove_backend;
/*4031a0: invalid weapon indices are no-ops. Clear only owned[weapon], then
 * scan active players via4a5b70; matching inventory and global85cce4 weapon
 * call4a70e0. Count, special weapon and notified player are reread after
 * callbacks. Owners remain alive, capacity bounds the mutable player list;
 * errors retain preceding removal/notifications. Ammo/current stay untouched. */
int rf_weapon_remove_owned(rf_weapon_inventory *,int32_t weapon,const rf_weapon_remove_backend *);

typedef struct rf_weapon_drop_source {
    int32_t current;uint32_t flags_1a8,handle,notification_owner;
    float position[3],extent_7c4;
} rf_weapon_drop_source;
typedef struct rf_weapon_drop_definition {int32_t ammo_type,quantity;} rf_weapon_drop_definition;
typedef struct rf_weapon_drop_pose {float position[3],basis[9];} rf_weapon_drop_pose;
typedef struct rf_weapon_drop_request {int32_t item,quantity;uint32_t owner;rf_weapon_drop_pose pose;} rf_weapon_drop_request;
typedef struct rf_weapon_drop_backend {
    uint32_t (*pose)(void *,rf_weapon_drop_pose *); /*418e60(actor,0,...); nonzero handled. */
    int32_t (*item)(void *,int32_t weapon); /*459a90. */
    uint32_t (*remote)(void *,int32_t item); /*5001d0: Remote Charges, low byte. */
    int32_t (*resolve_remote)(void *); /*459430: Remote Charge. */
    void (*remove)(void *,int32_t weapon); /*4031a0(actor+2a0,weapon). */
    /*4df1c0: identity local, radius.1, flags2000, FLT_MAX, hierarchy1. */
    int (*query)(void *,const float start[3],const float delta[3],rf_entity_death_drop_hit *);
    /*459100(item,empty,quantity,owner,position,basis,-1,0,0). */
    rf_entity_death_drop_item *(*create)(void *,const rf_weapon_drop_request *);
    void (*notify)(void *,uint32_t owner,int32_t item,const float point[3]); /*401340. */
    int (*bounds)(void *,uint32_t model,float *second_x); /*503310; read-only item. */
    void *context;
} rf_weapon_drop_backend;
/* Complete SP42ae10 orchestration with supplied resource owners. Parameter's
 * low byte==1 selects pose placement; any nonzero low byte notifies, including
 * failed creation. Other values remove inventory and clear current before query.
 * Callback/source/inventory/definition lifetimes span the call. No reentry.
 * Callbacks must preserve unrelated state; pose may change current, which is
 * reread for mapping/quantity. RNG/order and prior changes survive later errors.
 * Created item remains exposed on a bounds failure. No implicit allocation,
 * model evaluation, global RNG or live death dispatch. */
int rf_weapon_drop_sp(rf_weapon_drop_source *,rf_weapon_inventory *,const rf_weapon_drop_definition definitions[64],
    int32_t excluded,uint32_t parameter,rf_random_state *,const rf_weapon_drop_backend *,rf_entity_death_drop_item **);

typedef struct rf_weapon_empty_input {
    int32_t current,always_weapon,block_weapon,excluded_weapon,paired_first,paired_second;
    uint32_t automatic_enabled,request_flag,passenger,special_block,override_mode,defer_flag;
    uint32_t linked_present;
    int32_t linked_class;
} rf_weapon_empty_input;
enum { RF_WEAPON_EMPTY_NONE,RF_WEAPON_EMPTY_PAIR,RF_WEAPON_EMPTY_MESSAGE,RF_WEAPON_EMPTY_SELECT };
typedef struct rf_weapon_empty_action { int32_t kind,weapon; } rf_weapon_empty_action;
/* 4a6f41..4a70db after current-weapon resolution/presentation update. Caller
 * supplies resolved passenger (42acd0), projectile predicate (4c9e30) and linked
 * entity classification. Produces the original external action: paired firing
 * (4a4e80 flags 1,1), out-of-ammo message (4383c0), or select (4a4a50 flags 1,0).
 * Does not perform those operations. Inputs are stable snapshots after 4a5910.
 * Null primary inventory or negative current yields NONE. Action unchanged on
 * error. flags_264/count represent the 4c91b0 descriptor test. */
int rf_weapon_decide_empty(const rf_weapon_inventory *primary,const rf_weapon_inventory *linked,
    const rf_weapon_supply supply[64],const uint32_t flags_264[64],uint32_t weapon_count,
    const int32_t preference[32],const rf_weapon_empty_input *input,rf_weapon_empty_action *action);

typedef struct rf_weapon_descriptor {
    uint32_t flags_264,flags_268;
    int32_t release_sound_class; /* +204. */
} rf_weapon_descriptor;
typedef struct rf_weapon_reset_state {
    uint8_t active[64]; /* Entity +46c. */
    uint32_t flags_7d0,flags_810;
    int32_t sound_81c,sound_820,effect_13d4;
    uint32_t character_present,player_present;
} rf_weapon_reset_state;
typedef struct rf_weapon_reset_context {
    uint32_t disabled; /* Byte global 64ecbb, exact comparison with one. */
    uint32_t weapon_count; /* Global 872448, up to 64. */
} rf_weapon_reset_context;
typedef struct rf_weapon_reset_ops {
    int (*stop_sound)(void *user,int32_t handle);
    /* Owns 4285a0 position, 434d00 sound resolution and 48a9c0 emission. */
    int (*release_sound)(void *user,int32_t sound_class,int32_t *handle);
    int (*stop_effect)(void *user,int32_t handle); /* 48f130(handle,0). */
    int (*player_reset)(void *user); /* 4a6f10(entity+1430,0). */
} rf_weapon_reset_ops;
/* 41ae70 after a valid type-zero entity is resolved. Null state or indices
 * outside [0,63] are no-ops. descriptors has 64 entries even if count is lower.
 * character_present is 40a1e0, player_present is 42a8e0;
 * caller resolves them and callbacks may update the state through user.
 * Missing reached external operations return RF_NOT_FOUND at that point;
 * earlier side effects are retained. No implicit audio/effect/player no-ops.
 * This orchestrates reset; entity resolution and external adapters are separate. */
int rf_weapon_reset(rf_weapon_reset_state *state,int32_t weapon,
    const rf_weapon_descriptor descriptors[64],const rf_weapon_reset_context *context,
    rf_motion_playback_state *playback,const rf_motion_playback_resource *resources,uint32_t resource_count,
    const rf_weapon_reset_ops *ops,void *user);
#endif
