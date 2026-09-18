/* Include after scene_riot_shield_query.inc. Resolve the overlap between an
 * NPC's coarse body spheres and its held shield using current posed triangles.
 * No allocation, durability mutation, input or approximate extra geometry.
 */
static int scene_npc_shield_body_before(const scene_npc_shield_candidate *candidate,
    const float start[3],const float end[3],uint32_t *body_before_shield)
{
    rf_collision_model_part_query query={0};rf_collision_model_response_hit hit={0};
    const campaign_npc_body *npc;const campaign_model_owner *model;
    uint32_t i,accepted=0;int status;
    if(!candidate || !start || !end || !body_before_shield ||
       !campaign_npc_bodies || candidate->slot>=campaign_npc_body_count ||
       !campaign_model_owners || candidate->slot>=campaign_model_owner_count ||
       !isfinite(candidate->hit.time) || candidate->hit.time<0 || candidate->hit.time>1)return RF_RANGE;
    npc=campaign_npc_bodies+candidate->slot;
    if(!npc->registration.view || npc->registration.handle!=candidate->handle ||
       rf_entity_lookup(&campaign_entities,(int32_t)candidate->handle)!=&npc->view)return RF_NOT_FOUND;
    model=campaign_model_owners+candidate->slot;
    for(i=0;i<9;++i)if(!isfinite(model->basis[i]))return RF_RANGE;
    for(i=0;i<3;++i){
        if(!isfinite(start[i]) || !isfinite(end[i]) || !isfinite(model->position[i]))return RF_RANGE;
        query.input.start[i]=start[i];query.input.displacement[i]=end[i]-start[i];
        if(!isfinite(query.input.displacement[i]))return RF_RANGE;
    }
    memcpy(query.input.origin,model->position,12);memcpy(query.input.matrix,model->basis,36);
    /* model_pose_coordinates applies translation/rotation because flag2 is
     * clear. Keep original full segment so shield and body fractions agree.
     * First-hit is sufficient for this yes/no predicate; radius remains0. */
    query.input.flags=1;hit.time=candidate->hit.time;
    status=rf_scene_model_collision_query(candidate->slot,&query,&hit,0,&accepted);
    if(status)return status; /* Missing collision is unresolved, not a clear ray. */
    if(accepted && (!isfinite(hit.time) || hit.time<0 || !(hit.time<candidate->hit.time)))return RF_FORMAT;
    *body_before_shield=!!accepted;return RF_OK;
}
