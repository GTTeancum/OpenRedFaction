#ifndef RF_SCENE_HMG_AUDIO_INCLUDED
#define RF_SCENE_HMG_AUDIO_INCLUDED
/* Installed HMG primary/alternate are continuous_fire/alt_continuous_fire;
 * both Launch groups loop from0. Reuse the player's generation-qualified
 * firing owner instead of discarding a new voice at every accepted shot.
 * This bounded fix preserves immediate accepted-shot onset; it does not add
 * the authored Attack/.10s/Decay presentation or alter weapon cadence. */
static struct {int32_t weapon;uint32_t alternate;} scene_hmg_audio={-1,0};
uint32_t rf_scene_hmg_audio[8]; /* shots,episodes,closes,mode changes,retained shots,active,alternate,weapon */

static void scene_hmg_audio_publish(void)
{
    rf_scene_hmg_audio[5]=scene_hmg_audio.weapon>=0;
    rf_scene_hmg_audio[6]=scene_hmg_audio.alternate;
    rf_scene_hmg_audio[7]=(uint32_t)scene_hmg_audio.weapon;
}
static void scene_hmg_audio_close(void)
{
    if(scene_hmg_audio.weapon>=0) {
        /* Store the admitted ID: scene startup may replace the catalog before
         * resetting presentation. Never stop another weapon's shared owner. */
        scene_weapon_loop_audio_stop(scene_hmg_audio.weapon);
        ++rf_scene_hmg_audio[2];
    }
    scene_hmg_audio.weapon=-1;scene_hmg_audio.alternate=0;
    scene_hmg_audio_publish();
}
static void scene_hmg_audio_reset(void)
{
    scene_hmg_audio_close();memset(rf_scene_hmg_audio,0,sizeof(rf_scene_hmg_audio));
    scene_hmg_audio_publish();
}
static void scene_hmg_audio_tick(uint32_t admitted,uint32_t alternate)
{
    /* The caller supplies held/mode from scene_conventional_fire_select.
     * A mode switch ends the old episode immediately, even during cooldown;
     * only an actually accepted shot may begin the replacement sound. */
    if(scene_hmg_audio.weapon>=0 && (!admitted ||
       scene_hmg_audio.weapon!=campaign_extra_ids[1] ||
       scene_hmg_audio.alternate!=(alternate!=0))) {
        if(admitted && scene_hmg_audio.alternate!=(alternate!=0))++rf_scene_hmg_audio[3];
        scene_hmg_audio_close();
    }
}
static void scene_hmg_audio_shot(uint32_t alternate,const float position[3])
{
    int32_t weapon=campaign_extra_ids[1];
    ++rf_scene_hmg_audio[0];scene_hmg_audio_tick(1,alternate);
    if(weapon<0 || weapon>=64)return;
    if(scene_hmg_audio.weapon<0) {
        scene_hmg_audio.weapon=weapon;scene_hmg_audio.alternate=alternate!=0;
        ++rf_scene_hmg_audio[1];
    } else ++rf_scene_hmg_audio[4];
    scene_weapon_loop_audio_start(weapon,alternate?"HMG Launch 2":"HMG Launch 1",position);
    scene_hmg_audio_publish();
}
#endif
