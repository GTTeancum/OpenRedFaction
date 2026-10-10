#ifndef RF_SCENE_NPC_BULLET_IMPACT_AUDIO_INC
#define RF_SCENE_NPC_BULLET_IMPACT_AUDIO_INC
/* Include after scene_weapon_impact_audio.inc, before the NPC fragment helper.
 * RF.exe4c6463..4c6509: ordinary entity contact uses the dry material group,
 * retries Default only for sample-1, then plays once at projectile1b4. World
 * presentation4c5912..4c5996 supplies the equivalent existing dry service.
 * This is feedback from accepted port contacts, never a new damage decision.
 */
typedef struct scene_npc_bullet_contact {
    uint32_t kind; /* 0:none,1:world stopped ray,2:accepted detached piece */
    float limit,point[3];
} scene_npc_bullet_contact;

static uint32_t scene_npc_bullet_impact_ready(int32_t weapon)
{
    /* Precision/penetrating consumers have separate policies. Melee and
     * projectile weapons already fail the shared conventional allowlist. */
    return weapon!=campaign_sniper_id && weapon!=campaign_rail_id &&
        scene_weapon_impact_audio_ready(weapon);
}
static void scene_npc_bullet_impact_contact(int32_t weapon,int32_t material,const float point[3])
{
    if(scene_npc_bullet_impact_ready(weapon))
        scene_weapon_impact_audio_contact(weapon,material,point);
}
static void scene_npc_bullet_impact_ray(int32_t weapon,int32_t material,
    const float start[3],const float delta[3],float fraction)
{
    float point[3];uint32_t i;
    if(!scene_npc_bullet_impact_ready(weapon) || !isfinite(fraction) || fraction<0 || fraction>1)return;
    for(i=0;i<3;i++)point[i]=start[i]+delta[i]*fraction;
    scene_weapon_impact_audio_contact(weapon,material,point);
}
static void scene_npc_bullet_impact_stop(scene_stream *stream,int32_t weapon,
    const float start[3],const float delta[3],const scene_npc_bullet_contact *contact)
{
    if(!scene_npc_bullet_impact_ready(weapon) || !contact)return;
    if(contact->kind==1)scene_weapon_impact_audio_world(stream,weapon,start,delta,contact->limit);
    else if(contact->kind==2)scene_weapon_impact_audio_contact(weapon,-1,contact->point);
}
#endif
