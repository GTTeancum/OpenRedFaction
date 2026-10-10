#ifndef RF_SCENE_AI_SPIT_FLIGHT_INCLUDED
#define RF_SCENE_AI_SPIT_FLIGHT_INCLUDED
/* Include after scene_ai_laser.inc and guarded shared blast service.
 *4c6132 direct AP contact precedes4c62f5 independent explosive-kind radial.
 * Both use the accepted flight's identity/profile, never its current owner.
 * Physical shield and Nano consumption suppress radial even on shield break.
 * Ordinary expiry does not dispatch this callback and therefore never blasts. */

/* Spit's family-specific readiness and AP passage policy remain here;
 * the shared retained-contact reader contains neither. Cane energy5 must
 * never inherit this deliberately conservative Spit wet-start limitation. */
static uint32_t scene_ai_spit_impact_material(scene_stream *stream,
    const scene_ai_laser_projectile *shot,const scene_ai_laser_query *q,
    const rf_weapon_flight_contact *contact,int32_t *material)
{
    *material=-1;
    if(!scene_weapon_impact_audio_spit_ready(shot->weapon))return 0;
    if(q->kind==SCENE_LASER_WORLD || q->kind==SCENE_LASER_NPC_SHIELD ||
       q->kind==SCENE_LASER_PLAYER_SHIELD) {
        /*4c98b0: world/shield uses underwater groups for AP. Both Spit
         * rows have none. This shared flight retains first liquid passage,
         * not original launch-wet bit1: conservatively suppress after passage.
         * Initially submerged exit parity remains deferred; entity audio
         * below stays dry as4c6463 does. Do not infer wet from the victim. */
        if(!(shot->liquid.query_flags&0x1000u))return 0;
    }
    return scene_ai_contact_material(stream,shot,q,contact,material);
}
static int scene_ai_spit_contact(scene_stream *stream,uint32_t frame,const scene_ai_laser_projectile *shot,
    const scene_ai_laser_query *query,const rf_weapon_flight_contact *contact,
    uint32_t stats[12],const uint64_t *pool_epoch)
{
    uint64_t epoch=*pool_epoch;uint32_t consumed=0,audible,shield;int32_t material;int status;
    audible=scene_ai_spit_impact_material(stream,shot,query,contact,&material);
    shield=query->kind==SCENE_LASER_NPC_SHIELD || query->kind==SCENE_LASER_PLAYER_SHIELD;
    /*4c4f9b presents before physical-shield durability, including its breaking
     * hit. Selected stale owners fail the read-only qualification above. */
    if(audible && shield)scene_weapon_impact_audio_contact(shot->weapon,material,contact->hit.point);
    if(epoch!=*pool_epoch)return RF_OK;
    status=scene_ai_laser_contact_result(stream,frame,shot,query,contact,stats,pool_epoch,&consumed);
    if(status || consumed || epoch!=*pool_epoch)return status;
    /* Projectile+1b4 is passed directly at4c62ee and4c53a7. Do not inherit
     * the rocket adapter's unproven normal*.01 epicenter adjustment. The
     * direct victim is not excluded from the independent radial population. */
    if(shot->blast_radius>.1f) {
        status=scene_explosion_blast_source_guarded(stream,frame,contact->hit.point,
            shot->damage,shot->blast_radius,shot->source,shot->blast_kind,pool_epoch);
        if(status || epoch!=*pool_epoch)return status;
    }
    /*4c53f2 world and4c6463 entity playback follow direct/radial effects.
     * Nano's early consumed return stays silent, including its breaking hit.
     * Audio is void/best-effort and cannot reject accepted gameplay. */
    if(audible && !shield)scene_weapon_impact_audio_contact(shot->weapon,material,contact->hit.point);
    return RF_OK;
}
static int scene_ai_spit_tick(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    return scene_ai_energy_tick_contact(stream,frame,player_eye,scene_ai_spits,SCENE_AI_SPIT_CAPACITY,
        rf_scene_ai_spit,&scene_ai_spit_epoch,&scene_ai_spit_updating,scene_ai_spit_contact);
}
#endif
