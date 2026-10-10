#ifndef RF_SCENE_MACHINE_PISTOL_AUDIO_INCLUDED
#define RF_SCENE_MACHINE_PISTOL_AUDIO_INCLUDED
/* Installed Machine Pistol and Machine Pistol Special are two continuous_fire
 * definitions, each with its own primary Launch. Alt input changes the selected
 * definition; it is not an alternate shot. Keep the existing accepted-shot
 * onset and delegate the voice to the generation-qualified player loop owner.
 * Call close when a custom-mode transition STARTS, before its blocked interval.
 * No inventory, trigger, transition deadline or saved state is changed here. */
static struct {int32_t weapon;uint32_t special,owner;} scene_machine_pistol_audio={-1,0,0};
uint32_t rf_scene_machine_pistol_audio[9]; /* shots,episodes,closes,mode mismatches,retained shots,active,special,weapon,owner */

static void scene_machine_pistol_audio_publish(void)
{
    rf_scene_machine_pistol_audio[5]=scene_machine_pistol_audio.weapon>=0;
    rf_scene_machine_pistol_audio[6]=scene_machine_pistol_audio.special;
    rf_scene_machine_pistol_audio[7]=(uint32_t)scene_machine_pistol_audio.weapon;
    rf_scene_machine_pistol_audio[8]=scene_machine_pistol_audio.owner;
}
static void scene_machine_pistol_audio_close(void)
{
    if(scene_machine_pistol_audio.weapon>=0) {
        /* Store the admitted definition, not the current catalog or mode:
         * startup/load can replace both before clearing presentation state. */
        scene_weapon_loop_audio_stop(scene_machine_pistol_audio.weapon);
        ++rf_scene_machine_pistol_audio[2];
    }
    scene_machine_pistol_audio.weapon=-1;
    scene_machine_pistol_audio.special=scene_machine_pistol_audio.owner=0;
    scene_machine_pistol_audio_publish();
}
static void scene_machine_pistol_audio_reset(void)
{
    scene_machine_pistol_audio_close();
    memset(rf_scene_machine_pistol_audio,0,sizeof(rf_scene_machine_pistol_audio));
    scene_machine_pistol_audio_publish();
}
static void scene_machine_pistol_audio_tick(uint32_t admitted,uint32_t special)
{
    int32_t weapon=special?campaign_machine_special_id:campaign_extra_ids[0];
    /* Admission must reject pending custom transitions, not only mode changes.
     * This check runs before combat's special-weapon/transition early returns.
     * Closing never starts the replacement mode or retries a failed voice. */
    if(scene_machine_pistol_audio.weapon>=0 && (!admitted ||
       scene_machine_pistol_audio.weapon!=weapon ||
       scene_machine_pistol_audio.special!=(special!=0) ||
       scene_machine_pistol_audio.owner!=campaign_player_object.handle)) {
        if(admitted && scene_machine_pistol_audio.special!=(special!=0))
            ++rf_scene_machine_pistol_audio[3];
        scene_machine_pistol_audio_close();
    }
}
static void scene_machine_pistol_audio_shot(uint32_t special,const float position[3])
{
    int32_t weapon=special?campaign_machine_special_id:campaign_extra_ids[0];
    ++rf_scene_machine_pistol_audio[0];scene_machine_pistol_audio_tick(1,special);
    if(weapon<0 || weapon>=64)return;
    if(scene_machine_pistol_audio.weapon<0) {
        scene_machine_pistol_audio.weapon=weapon;
        scene_machine_pistol_audio.special=special!=0;
        scene_machine_pistol_audio.owner=campaign_player_object.handle;
        ++rf_scene_machine_pistol_audio[1];
    } else ++rf_scene_machine_pistol_audio[4];
    scene_weapon_loop_audio_start(weapon,
        special?"Machine Pistol Alt Launch":"Machine Pistol Launch",position);
    scene_machine_pistol_audio_publish();
}
#endif
