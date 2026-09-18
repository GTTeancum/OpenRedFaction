#ifndef RF_SCENE_PLAYER_SHIELD_DAMAGE_INC
#define RF_SCENE_PLAYER_SHIELD_DAMAGE_INC
/* Include AFTER scene_player_shield_lifecycle.inc and the physical
 * scene_player_shield_contact.inc, after campaign_ammo_publish declarations.
 * Incoming direct-ray path ONLY. Caller evaluates current view pose once
 * before enemy attacks; rendering must reuse it, not advance a second time.
 * Query before cover, test cover only up to candidate.hit.time, commit after
 * that test passes. Projectile/splash/script damage never calls this adapter.
 */
static scene_player_shield_state scene_player_shield_damage_state;
typedef struct scene_player_shield_candidate {
    rf_collision_model_response_hit hit;
    float forward[3],travel[3];uint32_t player;
} scene_player_shield_candidate;
/* intercepts, breaks, current durability bits, last incoming source */
uint32_t rf_scene_player_shield[4];
static void scene_player_shield_damage_reset(void)
{scene_player_shield_reset(&scene_player_shield_damage_state);memset(rf_scene_player_shield,0,sizeof(rf_scene_player_shield));}
static int scene_player_shield_damage_sync(void)
{
    uint32_t owned,selected;
    if(campaign_shield_id<0){scene_player_shield_reset(&scene_player_shield_damage_state);return RF_OK;}
    if(campaign_shield_id>=64)return RF_RANGE;
    owned=campaign_player_inventory.owned[campaign_shield_id]!=0;
    if(!owned){scene_player_shield_reset(&scene_player_shield_damage_state);return RF_OK;}
    selected=owned && campaign_equipped_slot==11 && !campaign_explicit_unarmed && campaign_player_damage.state.effects.health>0;
    return scene_player_shield_sync(&scene_player_shield_damage_state,&scene_npc_shields.definition,owned,selected);
}
static uint32_t scene_player_shield_damage_save_pending(void)
{
    return scene_player_shield_damage_state.owned &&
        scene_player_shield_damage_state.damage.health!=scene_npc_shields.definition.life;
}
static int scene_player_shield_query(scene_stream *stream,const float eye[3],const float eye_basis[9],
    const float start[3],const float end[3],float limit,scene_player_shield_candidate *out,uint32_t *accepted)
{
    static const float model_camera[3]={-.066f,.242f,-.154f};
    scene_player_shield_candidate candidate={0};uint32_t i,contact=0;int status;
    if(!stream || !eye || !eye_basis || !start || !end || !out || !accepted ||
       !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    *accepted=0;status=scene_player_shield_damage_sync();if(status)return status;
    if(!scene_player_shield_damage_state.selected || scene_player_shield_damage_state.broken)return RF_OK;
    if(!stream->player_weapon[11] || !stream->player_weapon[11]->initialized)return RF_NOT_FOUND;
    for(i=0;i<3;i++){candidate.forward[i]=eye_basis[6+i];candidate.travel[i]=end[i]-start[i];}
    /* Same directional eligibility as durable receive; avoid creating cover
     * candidates that would later reject their damage from behind. */
    if((double)candidate.forward[0]*candidate.travel[0]+(double)candidate.forward[1]*candidate.travel[1]+(double)candidate.forward[2]*candidate.travel[2]>=0)return RF_OK;
    status=scene_player_shield_contact(stream->player_weapon[11],model_camera,eye,eye_basis,start,end,limit,&candidate.hit,&contact);if(status)return status;
    if(contact){candidate.player=campaign_player_object.handle;*out=candidate;*accepted=1;}
    return RF_OK;
}
static int scene_player_shield_commit(const scene_player_shield_candidate *candidate,
    const rf_damage_request *request,uint32_t *accepted,uint32_t *broken)
{
    scene_player_shield_hit result;int status;uint32_t i,any=0;
    if(!candidate || !request || !accepted || !broken)return RF_RANGE;
    *accepted=*broken=0;
    if(candidate->player!=campaign_player_object.handle)return RF_NOT_FOUND;
    status=scene_player_shield_damage_sync();if(status)return status;
    status=scene_player_shield_receive(&scene_player_shield_damage_state,&scene_npc_shields.definition,1,1,
        candidate->forward,candidate->travel,request->amount,request->kind,&result);if(status)return status;
    *accepted=result.intercepted;*broken=result.broken;
    if(!result.intercepted)return RF_OK;
    ++rf_scene_player_shield[0];rf_scene_player_shield[1]+=result.broken;
    memcpy(rf_scene_player_shield+2,&scene_player_shield_damage_state.damage.health,4);rf_scene_player_shield[3]=request->source;
    if(result.remove_weapon){
        campaign_player_inventory.owned[campaign_shield_id]=0;campaign_player_inventory.loaded[campaign_shield_id]=0;
        /* Publish selects an existing supported weapon. Explicitly unarm if
         * none is owned; do not leave the destroyed shield selected. */
        for(i=0;i<scene_weapon_slots();i++){int32_t weapon=campaign_slot_weapon(i);
            if(scene_weapon_available(i) && weapon>=0 && weapon<64 && campaign_player_inventory.owned[weapon]){any=1;break;}}
        if(!any)campaign_explicit_unarmed=1;
        campaign_ammo_publish();
        status=scene_player_shield_sync(&scene_player_shield_damage_state,&scene_npc_shields.definition,0,0);if(status)return status;
    }
    return RF_OK;
}
#endif
