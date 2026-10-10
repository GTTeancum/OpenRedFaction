#ifndef RF_SCENE_ROBOT_DEATH_AUDIO_INCLUDED
#define RF_SCENE_ROBOT_DEATH_AUDIO_INCLUDED
/* Include after combat_pain_random and the ordinary audio backend. The
 * caller is already a RF_DAMAGE_PAIN_SOUND notification: core/entity.c keeps
 * original incoming/class-health > .001 and damage kind != 10 eligibility. */
static void scene_robot_death_sound(uint32_t slot,uint32_t handle,float fraction,int32_t now)
{
    campaign_npc_body *o;int32_t profile;rf_entity_damage_sound_state state={0};int status;
    campaign_pain_audio_context audio={&combat_pain_random,0,rf_scene_npc_pain_audio,0};
    rf_entity_damage_sound_backend backend={campaign_pain_audio_resolve,campaign_pain_audio_playing,campaign_pain_audio_play,&audio};
    profile=scene_robot_death_profile_at(slot);if(profile<0)return;
    o=scene_robot_death_identity(slot,handle);
    if(!o||!isfinite(o->damage.effects.health)||o->damage.effects.health>0||
       (o->damage.effects.flags_810&4u))return;
    state.health=o->damage.effects.health;state.flags=o->damage.effects.flags_810;
    state.death_descriptor=0;state.death_class=scene_robot_death_profiles[profile].death_group;
    memcpy(state.position,o->eye_position,12); /* Original41973f: actor+7d4. */
    for(uint32_t i=0;i<3;i++)if(!isfinite(state.position[i])){++rf_scene_robot_death[8];return;}
    /* Consume before optional callbacks. No retained owner dereference after
     * playback; failure remains silent and never prevents lethal gameplay. */
    o->view.flags_810|=4u;o->damage.effects.flags_810|=4u;
    ++rf_scene_robot_death[7];
    status=rf_entity_damage_sound(&state,fraction,0,0,now,&backend);
    if(status||audio.status)++rf_scene_robot_death[8];
}
static void scene_robot_death_foley(int32_t group,const float position[3])
{
    int32_t sample;campaign_pain_audio_context audio={&combat_sound_random,0,rf_scene_weapon_audio,0};
    if(group<0)return;sample=campaign_pain_audio_resolve(&audio,group);
    if(!audio.status)campaign_pain_audio_play(&audio,position,sample);
    if(audio.status)++rf_scene_robot_death[8];
}
#endif
