#ifndef RF_SCENE_MACHINE_PISTOL_CARRY_GAMEPLAY_INC
#define RF_SCENE_MACHINE_PISTOL_CARRY_GAMEPLAY_INC
/* Include after campaign import/export globals, before player_get/player_set.
 * Retain these sidecars across scene teardown; reset on new campaign, explicit
 * player_set(NULL), or a disk checkpoint load. They never own resources/ammo. */
#include "scene_machine_pistol_carry.inc"
static scene_machine_pistol_carry campaign_machine_export_sidecar;
static scene_machine_pistol_carry campaign_machine_import_sidecar;

static void scene_machine_carry_reset(void)
{
    scene_machine_pistol_carry_reset(&campaign_machine_export_sidecar);
    scene_machine_pistol_carry_reset(&campaign_machine_import_sidecar);
}
/* Call after publishing campaign_export_valid/state. Invalid ordinary export
 * also invalidates its paired state rather than retaining an older snapshot. */
static void scene_machine_carry_export(void)
{
    int status;
    scene_machine_pistol_carry_reset(&campaign_machine_export_sidecar);
    if(!campaign_export_valid || campaign_extra_ids[0]<0 || campaign_machine_special_id<0)return;
    status=scene_machine_pistol_carry_capture(&campaign_machine_export_sidecar,&campaign_player_export,
        &campaign_machine_mode,campaign_extra_ids[0],campaign_machine_special_id);
    if(status)scene_machine_pistol_carry_reset(&campaign_machine_export_sidecar);
}
/* Call only AFTER player_set accepts and copies the requested player state.
 * A disk load must reset sidecars instead: byte equality is not authorization
 * to revive session mode for a disk format which has no mode field. */
static void scene_machine_carry_stage(void)
{
    scene_machine_pistol_carry_reset(&campaign_machine_import_sidecar);
    if(scene_machine_pistol_carry_matches(&campaign_machine_export_sidecar,&campaign_player_import,
        campaign_extra_ids[0],campaign_machine_special_id))
        campaign_machine_import_sidecar=campaign_machine_export_sidecar;
}
/* Call after import validation/inventory assignment but BEFORE select_primary.
 * Resource readiness means both mode FP owners/custom clips loaded. No match
 * or unavailable resources retains ordinary base mode, never special stats
 * with the wrong model. Restoring held input avoids a spurious switch edge. */
static int scene_machine_carry_apply(const rf_campaign_player_state *imported,
    uint32_t resources_ready,uint32_t alternate_held)
{
    int status=RF_NOT_FOUND;
    if(!imported)return RF_RANGE;
    scene_machine_pistol_mode_reset(&campaign_machine_mode);
    if(resources_ready)status=scene_machine_pistol_carry_restore(&campaign_machine_import_sidecar,
        imported,campaign_extra_ids[0],campaign_machine_special_id,alternate_held,&campaign_machine_mode);
    scene_machine_pistol_carry_reset(&campaign_machine_import_sidecar);
    return status==RF_NOT_FOUND?RF_OK:status;
}
/* Additional existing RFCP scope veto, especially leftover special magazine
 * rounds after base removal. No changes to checkpoint bytes or disk schema. */
static uint32_t scene_machine_carry_disk_pending(void)
{
    if(campaign_extra_ids[0]<0 || campaign_machine_special_id<0)
        return campaign_machine_mode.special || campaign_machine_mode.pending;
    return scene_machine_pistol_disk_pending(&campaign_player_inventory,&campaign_machine_mode,
        campaign_extra_ids[0],campaign_machine_special_id);
}
#endif
