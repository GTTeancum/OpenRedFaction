#ifndef RF_SCENE_CROUCH_FIRE_ACTION_INCLUDED
#define RF_SCENE_CROUCH_FIRE_ACTION_INCLUDED
/*427f30 chooses from actual physical crouch, not requested stance. 42c3e2
 * falls back to fire_stand when the chosen authored clip is absent. This
 * pure selector does not start/restart clips, play Foley, or release shots. */
static int scene_crouch_fire_action(const int32_t actions[45],int32_t actor_action,
    int32_t property_4ec,uint32_t actor_flags,uint32_t alternate,int32_t *out)
{
    int32_t action;
    if(!actions||!out||alternate>1)return RF_RANGE;
    action=((actor_flags&0x400u)&&(actor_action==3||
        (actor_action==12&&property_4ec==1)))?4:(alternate?3:2);
    if(actions[action]< -1||actions[2]< -1)return RF_FORMAT;
    if(actions[action]==-1)action=2;
    if(actions[action]==-1)return RF_NOT_FOUND;
    *out=action;return RF_OK;
}
#endif
