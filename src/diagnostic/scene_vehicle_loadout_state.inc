#ifndef RF_SCENE_VEHICLE_LOADOUT_STATE_INCLUDED
#define RF_SCENE_VEHICLE_LOADOUT_STATE_INCLUDED
/* Source/class admission already succeeded at fresh owner construction. This
 * final source-derived normalization owns no allocation or fallible mutation;
 * saved health, pose, live flights and accepted ammunition remain untouched. */
static void scene_vehicle_loadout_refresh(scene_stream *s)
{
    int32_t weapons[2];uint32_t mask;
    if(!s||!s->driller_runtime||!s->driller)return;
    if(scene_vehicle_loadout_resolve((uint32_t)campaign_authored_vehicle_uid,rf_scene_vehicle_enabled,weapons,&mask))return;
    scene_vehicle_loadout_normalize(s,rf_scene_vehicle_enabled,weapons,mask,&s->driller_runtime->entry.view);
}
#endif
