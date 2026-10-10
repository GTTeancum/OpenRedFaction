#ifndef RF_SCENE_CONTINUOUS_CHARGE_CHECKPOINT_TYPES_INCLUDED
#define RF_SCENE_CONTINUOUS_CHARGE_CHECKPOINT_TYPES_INCLUDED
/* Runtime player identity is an admission receipt, never a serialized UID. */
typedef struct scene_continuous_charge_checkpoint_stage {
    scene_continuous_charge_state next,before;
    uint64_t epoch;
    uint32_t source,ready,locked;
} scene_continuous_charge_checkpoint_stage;
static int scene_continuous_charge_checkpoint_preflight(const void *,uint32_t);
static int scene_continuous_charge_checkpoint_capture(const scene_stream *,unsigned char *,uint32_t *);
static int scene_continuous_charge_checkpoint_prepare(const scene_stream *,const void *,uint32_t,
    const rf_campaign_player_state *,scene_continuous_charge_checkpoint_stage *);
static int scene_continuous_charge_checkpoint_validate(const scene_continuous_charge_checkpoint_stage *);
static void scene_continuous_charge_checkpoint_finish(scene_continuous_charge_checkpoint_stage *,uint32_t);
static int scene_continuous_charge_checkpoint_save_guard(const scene_stream *);
static int scene_continuous_charge_checkpoint_standalone(void);
#endif
