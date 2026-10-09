#ifndef RF_SCENE_LATE_LOAD_DIAGNOSTIC_INC
#define RF_SCENE_LATE_LOAD_DIAGNOSTIC_INC
/* Startup-only observations. Preserve the exact failing call and pre-cleanup
 * memory; teardown otherwise makes a loading failure look like free memory. */
uint32_t rf_scene_late_load[8]; /* stage,status,start pages,material count,material bytes,end pages,frame stage,UID */
static void scene_late_load_mark(uint32_t stage,const rf_materials *materials)
{
    rf_scene_campaign_load_stage=stage;rf_scene_late_load[0]=stage;
    rf_scene_late_load[1]=rf_scene_late_load[5]=rf_scene_late_load[6]=rf_scene_late_load[7]=0;
    rf_scene_late_load[3]=materials?materials->count:0;
    rf_scene_late_load[4]=materials?materials->allocated_bytes:0;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_late_load[2]=rf_xbox_available_memory_pages();
#endif
}
#endif
