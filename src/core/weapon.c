#include "rf/weapon.h"
int rf_weapon_reset(rf_weapon_reset_state *state,int32_t weapon,
    const rf_weapon_descriptor descriptors[64],const rf_weapon_reset_context *context,
    rf_motion_playback_state *playback,const rf_motion_playback_resource *resources,uint32_t resource_count,
    const rf_weapon_reset_ops *ops,void *user)
{
    int was_active,status; int32_t sound;
    if (!state || weapon<0 || weapon>=64) return RF_OK;
    if (!descriptors || !context || context->weapon_count>64) return RF_RANGE;
    was_active=state->active[weapon]!=0;
    if (was_active && (uint8_t)context->disabled!=1 && (descriptors[weapon].flags_264 & 6u)) {
        if (state->sound_81c!=-1) {
            if (!ops || !ops->stop_sound) return RF_NOT_FOUND;
            status=ops->stop_sound(user,state->sound_81c); if (status!=RF_OK) return status;
            state->sound_81c=-1;
        }
        if (descriptors[weapon].release_sound_class>-1) {
            if (!ops || !ops->release_sound) return RF_NOT_FOUND;
            status=ops->release_sound(user,descriptors[weapon].release_sound_class,&sound); if (status!=RF_OK) return status;
            state->sound_820=sound;
        }
        if (state->character_present && !(state->flags_810 & 1u)) {
            status=rf_motion_stop_nonlooping(playback,resources,resource_count); if (status!=RF_OK) return status;
        }
    }
    state->active[weapon]=0; state->flags_7d0 &= ~0x2000u;
    if ((uint32_t)weapon<context->weapon_count && (descriptors[weapon].flags_268 & 0x40u) && state->effect_13d4!=-1) {
        if (!ops || !ops->stop_effect) return RF_NOT_FOUND;
        status=ops->stop_effect(user,state->effect_13d4); if (status!=RF_OK) return status;
    }
    /* 41afbb..41b015 performs read-only player/weapon lookups (42a8e0,
     * 4c90f0,48aa30,48aa90) and discards their results. 48aa90 returns a
     * player pointer; it is not a release operation. Stable views permit
     * omitting this block without changing reset effects. */
    if (state->player_present) {
        if (!ops || !ops->player_reset) return RF_NOT_FOUND;
        status=ops->player_reset(user); if (status!=RF_OK) return status;
    }
    return RF_OK;
}
