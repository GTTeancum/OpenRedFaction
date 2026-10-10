#ifndef RF_SCENE_AI_TRIBEAM_FLIGHT_INCLUDED
#define RF_SCENE_AI_TRIBEAM_FLIGHT_INCLUDED
/* Include after scene_ai_laser.inc. Same direct-contact primitive and current
 * shield/cover ordering; independent slots, counters, reset epoch and lifetime. */
static int scene_ai_tribeam_tick(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    return scene_ai_energy_tick(stream,frame,player_eye,scene_ai_tribeams,SCENE_AI_TRIBEAM_CAPACITY,
        rf_scene_ai_tribeam,&scene_ai_tribeam_epoch,&scene_ai_tribeam_updating);
}
#endif
