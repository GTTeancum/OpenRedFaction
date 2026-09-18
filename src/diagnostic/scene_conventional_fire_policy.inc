/* First-pass installed SP firearm modes. Shared primary parser supplies all
 * numerical settings; explicit named kinds prevent its alternate defaults
 * from inventing an alternate attack. Machine Pistol alt_custom_mode has only
 * custom_start/custom_leave presentation in weapons.tbl, no alternate shot
 * settings. Report that request for caller handling, never fire it as a gun.
 * Scoped rifle alt_zoom requests sight behavior only. HMG has a real slower,
 * tighter alternate attack. Parent owns ammo debit, reload, effects and ray
 * obstruction; feed this policy to the existing spread/body damage services.
 * Spread is authored degrees (MP Flags2 undeviating remains a separate caller
 * interpretation), not a new spread distribution. */
#include "rf/entity_assets.h"
#include <math.h>
enum {
    SCENE_CONVENTIONAL_MACHINE_PISTOL=0,
    SCENE_CONVENTIONAL_HEAVY_MACHINE_GUN=1,
    SCENE_CONVENTIONAL_SCOPED_RIFLE=2
};
typedef struct scene_conventional_fire_policy {
    rf_weapon_trigger_rules trigger;
    uint32_t held,alternate,custom_requested,zoom_requested,projectiles;
    float damage,spread_degrees;int32_t damage_kind;
} scene_conventional_fire_policy;
static int scene_conventional_fire_select(uint32_t kind,
    const rf_weapon_primary_definition *definition,uint32_t primary,
    uint32_t alternate,scene_conventional_fire_policy *out)
{
    scene_conventional_fire_policy policy={0};float seconds;
    if(!definition || !out || kind>SCENE_CONVENTIONAL_SCOPED_RIFLE || primary>1 || alternate>1)
        return RF_RANGE;
    policy.alternate=kind==SCENE_CONVENTIONAL_HEAVY_MACHINE_GUN && alternate && !primary;
    policy.custom_requested=kind==SCENE_CONVENTIONAL_MACHINE_PISTOL && alternate && !primary;
    policy.zoom_requested=kind==SCENE_CONVENTIONAL_SCOPED_RIFLE && alternate;
    policy.held=primary || policy.alternate;
    seconds=policy.alternate?definition->alt_fire_seconds:definition->fire_seconds;
    policy.damage=policy.alternate?definition->alt_damage:definition->damage;
    policy.spread_degrees=policy.alternate?definition->alt_spread_degrees:definition->spread_degrees;
    policy.projectiles=definition->projectiles;policy.damage_kind=definition->damage_kind;
    if(!isfinite(seconds) || seconds<=0 || seconds>60 ||
       !isfinite(policy.damage) || policy.damage<=0 ||
       !isfinite(policy.spread_degrees) || policy.spread_degrees<0 || policy.spread_degrees>180 ||
       !policy.projectiles || policy.projectiles>32 || !definition->burst_count ||
       !isfinite(definition->burst_seconds) || definition->burst_seconds<0 || definition->burst_seconds>60)
        return RF_RANGE;
    policy.trigger.fire_ticks=(uint32_t)ceilf(seconds*60.f);
    policy.trigger.burst_count=policy.alternate?1:definition->burst_count;
    policy.trigger.burst_ticks=(uint32_t)ceilf(definition->burst_seconds*60.f);
    policy.trigger.semi_automatic=policy.alternate?0:definition->semi_automatic;
    *out=policy;return RF_OK;
}
