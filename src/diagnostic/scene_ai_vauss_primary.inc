#ifndef RF_SCENE_AI_VAUSS_PRIMARY_INCLUDED
#define RF_SCENE_AI_VAUSS_PRIMARY_INCLUDED
/* Include after scene_ai_weapon_selection.inc. Exact ordinary NPC Vauss,
 * not a mounted host, Tankbot burst or a generic clipless-weapon fallback.
 * Original weapons.tbl1709 authors continuous_fire/from_eye, turret_ammo,
 * finite capacity170, no magazine/reload and no burst. Read its real primary
 * scalars once; startup/loadout already owns the selected weapon and reserve. */
static struct {
    rf_weapon_primary_definition primary;
    int32_t weapon;uint32_t ready;
} scene_ai_vauss_primary;

static int campaign_enemy_vauss_primary_open(rf_vpp *tables,
    const rf_weapon_supply_catalog *supply)
{
    rf_weapon_primary_definition primary;const rf_weapon_acquire_definition *ammo;
    int32_t weapon;int status;
    memset(&scene_ai_vauss_primary,0,sizeof(scene_ai_vauss_primary));
    scene_ai_vauss_primary.weapon=-1;
    if(!tables || !supply || supply->names.count>64 ||
       supply->names.primary_count>supply->names.count)return RF_RANGE;
    weapon=rf_weapon_name_find(&supply->names,"Vauss");
    if(weapon<0)return RF_NOT_FOUND;
    if((uint32_t)weapon>=supply->names.primary_count)return RF_FORMAT;
    ammo=supply->definitions+weapon;
    if(ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine!=0 ||
       ammo->capacity!=170)return RF_FORMAT;
    status=rf_weapon_primary_load(tables,"Vauss",128*1024,&primary);if(status)return status;
    if(primary.magazine!=0 || primary.reload_seconds!=0 ||
       primary.projectiles!=1 || primary.damage_kind!=1 ||
       primary.burst_count!=1 || primary.burst_seconds!=0)return RF_FORMAT;
    scene_ai_vauss_primary.primary=primary;
    scene_ai_vauss_primary.weapon=weapon;scene_ai_vauss_primary.ready=1;
    return RF_OK;
}
static uint32_t campaign_enemy_vauss_primary_weapon(int32_t weapon)
{return scene_ai_vauss_primary.ready && weapon==scene_ai_vauss_primary.weapon;}

static int campaign_enemy_vauss_primary_select(const campaign_npc_body *owner,
    const rf_weapon_supply_catalog *supply,campaign_enemy_weapon_selection *out)
{
    campaign_enemy_weapon_selection selected={0};int32_t weapon;
    if(!owner || !supply || !out || supply->names.count>64)return RF_RANGE;
    weapon=owner->view.weapons[0];
    if(!campaign_enemy_vauss_primary_weapon(weapon))return RF_NOT_FOUND;
    if(weapon<0 || weapon>=64 || (uint32_t)weapon>=supply->names.count ||
       !owner->inventory.owned[weapon])return RF_NOT_FOUND;
    selected.primary=&scene_ai_vauss_primary.primary;
    selected.ammo=supply->definitions+weapon;
    if(selected.ammo->ammo_type<0 || selected.ammo->ammo_type>=32 ||
       selected.ammo->magazine!=0 || selected.ammo->capacity!=170)return RF_FORMAT;
    /* No player slot alias and no world penetration. The accepted-shot
     * adapter consumes authored actor power after the ordinary spread draw. */
    selected.slot=UINT32_MAX;selected.reserve_fed=1;*out=selected;return RF_OK;
}
static int campaign_enemy_vauss_primary_ready(const rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *ammo,int32_t weapon,uint32_t *reload_due,
    int32_t *reload_weapon,uint32_t *ready,uint32_t *event)
{
    if(!inventory || !ammo || !reload_due || !reload_weapon || !ready || !event ||
       !campaign_enemy_vauss_primary_weapon(weapon) || weapon<0 || weapon>=64 ||
       !inventory->owned[weapon] || ammo->ammo_type<0 || ammo->ammo_type>=32 ||
       ammo->magazine!=0 || ammo->capacity!=170 || inventory->loaded[weapon]!=0 ||
       inventory->reserve[ammo->ammo_type]<0)return RF_RANGE;
    /* Cancel a previous magazine reload without transferring any ammunition.
     * The ordinary accepted-shot owner calls rf_weapon_consume_shot once;
     * zero reserve follows its existing event3 exhaustion/fallback policy. */
    *reload_due=0;*reload_weapon=-1;
    *ready=inventory->reserve[ammo->ammo_type]>0;*event=*ready?0:3;
    return RF_OK;
}
#endif
