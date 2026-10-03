#ifndef RF_SCENE_VEHICLE_PROFILE_PACK_DECL_INCLUDED
#define RF_SCENE_VEHICLE_PROFILE_PACK_DECL_INCLUDED
/* Opaque before scene_stream; complete type follows ground runtime helpers. */
typedef struct scene_vehicle_profile_pack scene_vehicle_profile_pack;
static int scene_vehicle_profile_pack_open(uint32_t,const scene_driller_resources *,rf_vpp *,rf_vpp *,rf_vpp *,uint32_t,float,uint32_t,scene_vehicle_profile_pack **);
static void scene_vehicle_profile_pack_close(scene_vehicle_profile_pack **);
static int scene_vehicle_profile_pack_materials_merge(scene_vehicle_profile_pack *,rf_materials *,uint32_t,uint32_t);
#endif
