#ifndef RF_SCENE_PLAYER_SHOT_HEARING_INC
#define RF_SCENE_PLAYER_SHOT_HEARING_INC
#include "rf/vpp.h"
/* Existing flag vocabulary:4c2dce/513020, flags_264 bit15 silent, bit5 melee.
 * Authored weapons.tbl documents silent as no firing noise. Port policy:
 * attached Undercover suppressor likewise emits no shot-awareness event.
 * Ordinary shots retain the existing16m first-pass radius. This admission
 * never suppresses sound playback, impacts, damage reactions or visual LOS.
 * Invoke once per accepted shot, outside pellet loops. */
typedef struct scene_player_shot_hearing_policy {
    uint32_t emit;float radius;
} scene_player_shot_hearing_policy;
static int scene_player_shot_hearing(uint32_t flags_264,int32_t selected_weapon,
    int32_t undercover_weapon,uint32_t attached,uint32_t accepted_shot,
    scene_player_shot_hearing_policy *result)
{
    scene_player_shot_hearing_policy value={0};
    if(!result || attached>1 || accepted_shot>1 || selected_weapon<0 || selected_weapon>=64 ||
        undercover_weapon< -1 || undercover_weapon>=64)return RF_RANGE;
    if(accepted_shot && !(flags_264&((1u<<15)|(1u<<5))) &&
       !(attached && undercover_weapon>=0 && selected_weapon==undercover_weapon)) {
        value.emit=1;value.radius=16.f;
    }
    *result=value;return RF_OK;
}
#endif
