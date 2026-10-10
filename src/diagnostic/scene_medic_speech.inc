/*4bb2e6 calls4299b0, the same per-actor1400 voice tested by429a50.
 * Flat/radio/subtitle history is not actor speech. Expired native handles
 * cannot gate admission; optional success speech is omitted in this slice. */
static uint32_t scene_medic_speech_active(const campaign_npc_body *owner)
{
    uint32_t uid;
    if(!scene_medic_live(owner))return 0;
    uid=(uint32_t)campaign_seeds.records.items[owner-campaign_npc_bodies].record.uid;
    for(uint32_t i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++){
        const scene_message_voice *voice=campaign_message_voices+i;
        if(voice->route==SCENE_MESSAGE_ACTOR&&voice->key==uid&&campaign_message_active(voice))return 1;
    }
    return 0;
}
