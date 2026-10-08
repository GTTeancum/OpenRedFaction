#ifndef RF_SCENE_VEHICLE_HEADLAMP_DECL_INCLUDED
#define RF_SCENE_VEHICLE_HEADLAMP_DECL_INCLUDED
static void scene_vehicle_headlamp_close(void);
static uint32_t scene_vehicle_headlamp_parent(uint32_t);
static uint32_t scene_vehicle_headlamp_initial(uint32_t);
static int scene_vehicle_headlamp_set(void *,uint32_t,uint32_t);
static int scene_vehicle_headlamp_open(scene_stream *,const char *);
static int scene_vehicle_headlamp_tick(scene_stream *);
static void scene_vehicle_headlamp_assign(scene_stream *);
static uint32_t scene_vehicle_headlamp_legacy_save_allowed(scene_stream *);
static int scene_vehicle_headlamp_visible(scene_stream *,uint32_t,uint32_t *);
#endif
