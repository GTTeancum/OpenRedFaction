#ifndef RF_SCENE_CONTINUOUS_CHARGE_STATE_INCLUDED
#define RF_SCENE_CONTINUOUS_CHARGE_STATE_INCLUDED
/* Gameplay projection of the local player's shared FP0/1/2 and f90 owners.
 * No renderer pointers, clip playback or inventory are duplicated here. */
typedef struct scene_continuous_charge_state {
    int32_t current,pending;float duration,elapsed;
    int32_t due_ms,last_profile,episode_weapon;uint32_t episode_mode;
} scene_continuous_charge_state;
static scene_continuous_charge_state scene_continuous_charge={-1,-1,0,0,-1,-1,-1,0};
static struct {
    uint64_t epoch,prepared_epoch;
    uint32_t source,prepared_source,prepared_frame,serviced_frame,prepared,serviced,busy,suspended;
    int32_t debit_weapon;
} scene_continuous_charge_runtime;
/* periodic services/debits, Riot attempt receipts/debits, stops, selections,
 * profile binds, frame, current, pending, deadline, profile, episode/mode. */
uint32_t rf_scene_continuous_charge[14];
static int scene_continuous_charge_host_weapon(const scene_stream *,int32_t *,uint32_t *);
static int scene_continuous_charge_profile_admit(const scene_stream *,int32_t);
static int scene_continuous_charge_live_admit(void);
static uint32_t scene_continuous_charge_busy(void);
static uint32_t scene_continuous_charge_is_constructor(void);
static int32_t scene_continuous_charge_now(void);
static void scene_continuous_charge_restore_scope(uint32_t);
static void scene_continuous_charge_restore_publish(const scene_continuous_charge_state *);
static void scene_continuous_charge_reset_player(void);
static void scene_continuous_charge_stop(void);
static void scene_continuous_charge_selected(void);
static void scene_continuous_charge_accept(int32_t,uint32_t);
static void scene_continuous_charge_attempt(uint32_t,int32_t);
#endif
