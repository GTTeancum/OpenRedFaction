/* Ownership-only acquisition for the resolved riot shield weapon ID.
 * Installed items.tbl: class "riot shield", Count1, Gives Weapon "riot shield".
 * Its supply descriptor has ammo_type-1/capacity0 and a legacy magazine32;
 * neither count nor magazine represents ammunition or shield durability.
 * Parent branches here only for its resolved shield ID. Ordinary weapon/ammo
 * pickups retain rf_weapon_pickup_grant_sp. No selection or durability reset:
 * sync the player shield lifecycle after ownership publication, and retire a
 * world pickup only if acquired. Duplicate owned shields do not repair/refill.
 * Like ordinary grants, Gives Weapon can acquire with quantity0; quantity is
 * validated but never multiplied or used to index an ammunition pool. */
#include "rf/weapon.h"
static int scene_player_shield_pickup_grant(rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *definition,int32_t shield_weapon,
    int32_t quantity,uint32_t gives_weapon,rf_weapon_pickup_grant *out)
{
    rf_weapon_pickup_grant grant={0};
    if(!inventory || !definition || !out || shield_weapon<0 || shield_weapon>=64 ||
       quantity<0 || gives_weapon>1 || definition->ammo_type!=-1 ||
       definition->capacity!=0 || definition->magazine<0)return RF_RANGE;
    if(gives_weapon && !inventory->owned[shield_weapon]){
        inventory->owned[shield_weapon]=1;grant.acquired=1;
    }
    *out=grant;return RF_OK;
}
