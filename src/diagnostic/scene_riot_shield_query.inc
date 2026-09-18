/* Include after scene_riot_shield_gameplay.inc. Read-only physical shield
 * candidate discovery, independent of actor-body silhouette. Caller applies
 * live/visible actor eligibility, world/rubble obstruction and nearest-hit
 * ordering before committing the selected candidate. Same frame/pose only.
 */
typedef struct scene_npc_shield_candidate {
    uint32_t slot,handle;float limit;
    rf_collision_model_response_hit hit;
} scene_npc_shield_candidate;
static int scene_npc_shield_query(uint32_t slot,const float start[3],const float end[3],
    float limit,scene_npc_shield_candidate *result,uint32_t *accepted)
{
    const campaign_npc_body *npc;const scene_riot_shield_owner *owner;
    scene_npc_shield_candidate candidate={0};rf_weapon_hand_placement pose;
    uint32_t model,contact=0,i;double dot;float health,travel[3];const float *forward;int status;
    if(!result || !accepted || !start || !end || !isfinite(limit) || limit<0 || limit>1 ||
       !campaign_npc_bodies || slot>=campaign_npc_body_count ||
       !scene_npc_shields.owners || slot>=scene_npc_shields.count)return RF_RANGE;
    npc=campaign_npc_bodies+slot;
    if(!npc->registration.view || rf_entity_lookup(&campaign_entities,(int32_t)npc->registration.handle)!=&npc->view)return RF_NOT_FOUND;
    owner=scene_npc_shields.owners+slot;
    health=owner->initialized && owner->handle==npc->registration.handle?owner->damage.health:scene_npc_shields.definition.life;
    if(npc->view.weapons[0]!=scene_npc_shields.weapon || !npc->inventory.owned[scene_npc_shields.weapon] || health<=0){*accepted=0;return RF_OK;}
    if(!campaign_model_owners || slot>=campaign_model_owner_count)return RF_NOT_FOUND;
    forward=campaign_model_owners[slot].basis+6;
    for(i=0;i<3;++i){
        if(!isfinite(start[i]) || !isfinite(end[i]) || !isfinite(forward[i]))return RF_RANGE;
        travel[i]=end[i]-start[i];if(!isfinite(travel[i]))return RF_RANGE;
    }
    dot=(double)forward[2]*travel[2]+(double)forward[1]*travel[1]+(double)forward[0]*travel[0];
    if(!(dot<0)){*accepted=0;return RF_OK;}
    model=campaign_weapon_models.weapons[scene_npc_shields.weapon].model;
    if(!model || model>campaign_weapon_models.count)return RF_NOT_FOUND;
    status=rf_scene_npc_weapon_placement(npc->registration.handle,0,&pose);if(status)return status;
    status=scene_riot_shield_contact(&campaign_weapon_models.items[model-1].render,&pose,start,end,limit,&candidate.hit,&contact);
    if(status)return status;
    if(contact){candidate.slot=slot;candidate.handle=npc->registration.handle;candidate.limit=limit;*result=candidate;}
    *accepted=contact;return RF_OK;
}
/* Commit only the winner after world/rubble checks. Keep full endpoints and
 * ORIGINAL query limit: passing hit.time as receive's limit rejects the exact
 * same surface because the underlying trace is strictly-nearer. Revalidation
 * is allocation-free and rejects a replaced owner. No state writes on query.
 */
static int scene_npc_shield_commit(const scene_npc_shield_candidate *candidate,
    const float start[3],const float end[3],float damage,int32_t type,uint32_t *accepted,uint32_t *broken)
{
    if(!candidate || !accepted || !broken || !campaign_npc_bodies || candidate->slot>=campaign_npc_body_count)return RF_RANGE;
    if(campaign_npc_bodies[candidate->slot].registration.handle!=candidate->handle)return RF_NOT_FOUND;
    return scene_npc_shield_receive(candidate->slot,start,end,candidate->limit,damage,type,accepted,broken);
}
