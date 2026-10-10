#ifndef RF_SCENE_AI_VAUSS_PENETRATION_INC
#define RF_SCENE_AI_VAUSS_PENETRATION_INC
/* Exact accepted ordinary NPC Vauss consumer. Keep weapon identity at this
 * boundary, then share the unchanged fresh-base/full-ray actor policy.
 * No ammo, cadence, spread, presentation or inventory substitution here. */
#include "scene_ai_actor_penetration.inc"
static int scene_ai_vauss_penetration_fire(scene_stream *stream,uint32_t frame,
    const campaign_npc_body *accepted_owner,uint32_t accepted_slot,
    const scene_ai_actor_penetration_shot *shot,const float eye[3],const float ray[3],
    const float player_eye[3],uint32_t clock_bits,const rf_damage_effect_backend *effects,
    combat_feedback *feedback,scene_ai_actor_penetration_result *out)
{
    if(!shot || !campaign_enemy_vauss_primary_weapon(shot->weapon))return RF_RANGE;
    return scene_ai_actor_penetration_fire(stream,frame,accepted_owner,accepted_slot,
        shot,eye,ray,player_eye,clock_bits,effects,feedback,out);
}
#endif
