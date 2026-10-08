#ifndef RF_SCENE_VEHICLE_ALLEGIANCE_DECL_INCLUDED
#define RF_SCENE_VEHICLE_ALLEGIANCE_DECL_INCLUDED
extern uint32_t rf_scene_vehicle_allegiance[8]; /* calls,selected,passive,handle,before,after,UID,reserved */
typedef struct scene_vehicle_allegiance_stage scene_vehicle_allegiance_stage;
static int scene_vehicle_set_friendliness(uint32_t,uint32_t);
static uint32_t scene_vehicle_allegiance_legacy_save_allowed(void);
static uint32_t scene_vehicle_allegiance_bytes(const scene_vehicle_allegiance_stage *);
static void scene_vehicle_allegiance_close(scene_vehicle_allegiance_stage **);
static int scene_vehicle_allegiance_capture(unsigned char *,uint32_t,uint32_t,uint32_t *);
static int scene_vehicle_allegiance_prepare(const unsigned char *,uint32_t,uint32_t,
    scene_vehicle_allegiance_stage **,const unsigned char **,uint32_t *);
static void scene_vehicle_allegiance_assign(const scene_vehicle_allegiance_stage *);
#endif
