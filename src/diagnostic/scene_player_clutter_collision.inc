#ifndef RF_SCENE_PLAYER_CLUTTER_COLLISION_INC
#define RF_SCENE_PLAYER_CLUTTER_COLLISION_INC
/* Ordinary player-only contacts with stationary authored type-4 owners.
 * Include after clutter/group-control ownership and before player sweeps.
 * No allocation, prop response/damage, support owner or retained query state.
 * The original mesh tracer owns triangle sidedness and finite-radius contact.
 */
static int scene_player_clutter_stationary(uint32_t slot,const rf_clutter_base_owner *owner)
{
    const rf_physics_body_state *body=&owner->body.state;
    const rf_level_clutter *authored=campaign_clutter_records.items+slot;uint32_t i;
    /* These are factory parent properties, not an object handle. The current
     * static factory supplies 0/1; other parent families are outside this slice. */
    if(owner->parent_byte || owner->parent_group!=1)return 0;
    for(i=0;i<campaign_group_control_child_count;i++)
        if(campaign_group_control_children[i].kind==SCENE_GROUP_CONTROL_CLUTTER &&
           campaign_group_control_children[i].source==slot)return 0;
    /* Group membership survives its transient pose-dirty bit. Separately
     * consult the actual attachment node when it has been published. Before
     * the first attachment pass the ordinary base factory is unparented. */
    if(campaign_attachment_nodes){
        const rf_attachment_node *node;
        if(slot>=campaign_attachment_count)return 0;
        node=campaign_attachment_nodes+slot;
        if(node->owner && (node->owner!=&owner->state || node->flags!=&owner->state.flags ||
           node->parent!=UINT32_MAX))return 0;
    }
    /* Keep this authored-stationary slice within the existing RFPC pose
     * contract; pose_set teleports need their own future save/physics owner. */
    for(i=0;i<3;i++)
        if(!isfinite(owner->state.position[i]) || owner->state.position[i]!=authored->position[i] ||
           body->position[i]!=owner->state.position[i] ||
           body->next_position[i]!=owner->state.position[i] || body->velocity[i]!=0 ||
           body->vector_c8[i]!=0 || body->mass_vector_d4[i]!=0)return 0;
    for(i=0;i<9;i++)
        if(!isfinite(owner->matrix[i]) || owner->matrix[i]!=authored->matrix[i/3][i%3] || body->orientation[i]!=owner->matrix[i] ||
           body->next_orientation[i]!=owner->matrix[i])return 0;
    return 1;
}

/* Slabs are a rejection only. Use double expansion and an exact zero-delta
 * branch so tiny approaches and tangencies cannot disappear in a broadphase
 * epsilon. Physics spheres need not enclose the render/collision mesh. */
static int scene_player_clutter_bounds(const float start[3],const float delta[3],float radius,
    const double minimum[3],const double maximum[3],float limit)
{
    double lo=0,hi=limit;uint32_t i;
    for(i=0;i<3;i++){
        double lower=minimum[i]-radius,upper=maximum[i]+radius,a,b;
        if(delta[i]==0){if(start[i]<lower || start[i]>upper)return 0;continue;}
        a=(lower-start[i])/delta[i];b=(upper-start[i])/delta[i];
        if(a>b){double swap=a;a=b;b=swap;}
        if(a>lo)lo=a;if(b<hi)hi=b;if(lo>hi)return 0;
    }
    return 1;
}

/* Compose AFTER the existing world/piece/vehicle consumers: a strict nearer
 * hit wins, and every existing contact (including exact ties) is preserved.
 * replaced is optional; ground callers use it to discard a superseded loose
 * piece support reference. On error result/matched/replaced stay untouched.
 * solid remains UINT32_MAX and velocity is zero: static support, not a newly
 * invented movable-prop owner. object_id is the live generation-bearing handle.
 */
static int scene_player_clutter_compose(uint32_t actor,uint32_t parent,
    const rf_collision_body_query *query,rf_geometry_body_hit *result,uint32_t *matched,uint32_t *replaced)
{
    rf_geometry_body_hit value={0};float delta[3],limit;uint32_t i,j,k,any=0;int status;
    if(!query || !result || !matched || (query->count&&!query->spheres) || query->count>64 ||
       !isfinite(query->limit) || query->limit<0 || query->limit>1 ||
       (campaign_group_control_child_count&&!campaign_group_control_children))return RF_RANGE;
    limit=query->limit;
    if(*matched){
        if(!isfinite(result->contact.fraction) || result->contact.fraction<0 || result->contact.fraction>1)return RF_FORMAT;
        if(result->contact.fraction<limit)limit=result->contact.fraction;
    }
    /* Do not turn this into the NPC pair scheduler. The supplied source must
     * still be the exact registered ordinary type-0 player (object flag8). */
    if(!campaign_spawn || campaign_player_object.handle!=actor ||
       campaign_player_object.view!=&campaign_player_view || campaign_player_view.type!=0 ||
       !(campaign_player_view.flags_7c&8u) || (campaign_player_view.flags_7c&(2u|0x4000u|0x40000u)) ||
       rf_object_registry_lookup(&campaign_registry,actor)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)actor)!=&campaign_player_view)goto done;
    if(!query->count || !limit || !campaign_clutter_bodies)goto done;
    for(i=0;i<3;i++){
        if(!isfinite(query->start[i]) || !isfinite(query->end[i]))return RF_FORMAT;
        delta[i]=query->end[i]-query->start[i];if(!isfinite(delta[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(query->matrix[i][j]))return RF_FORMAT;
    }
    for(i=0;i<query->count;i++){
        if(!isfinite(query->spheres[i].radius) || query->spheres[i].radius<0)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(query->spheres[i].center[j]))return RF_FORMAT;
    }
    if(delta[0]==0 && delta[1]==0 && delta[2]==0)goto done;
    for(i=0;i<campaign_clutter_records.count;i++){
        const rf_clutter_base_owner *owner=campaign_clutter_bodies[i];
        double minimum[3],maximum[3];uint32_t slot,model;
        if(!owner || owner->state.handle==actor || owner->state.handle==parent ||
           (owner->state.flags&(2u|0x4000u|0x40000u)) ||
           rf_object_registry_lookup(&campaign_registry,owner->state.handle)!=&owner->state)continue;
        if(owner->state.class_index<0 || (uint32_t)owner->state.class_index>=campaign_clutter_classes.count ||
           !campaign_clutter_classes.items || owner->state.definition!=campaign_clutter_classes.items+owner->state.class_index)return RF_FORMAT;
        /* 40f360/48be00: collide_weapon sets object40000 even when the class
         * also has collide_object. This partial scene factory retains the
         * class facts, so do not depend on its absent full-factory flag. */
        if(!(owner->state.definition->flags&4u) || (owner->state.definition->flags&2u) ||
           !((scene_actor_body.state.flags|owner->body.state.flags)&0x20u) ||
           !(owner->body.state.bounds.radius>.5f))continue;
        if(!campaign_clutter_records.items || !campaign_clutter_model_slots ||
           !campaign_clutter_shared || !campaign_clutter_models)return RF_FORMAT;
        if(!scene_player_clutter_stationary(i,owner))continue;
        if(!isfinite(owner->body.state.bounds.radius) || !isfinite(owner->attachment.radius) || owner->attachment.radius<0)return RF_FORMAT;
        if(owner->material>=10 || owner->material!=owner->state.definition->material)return RF_FORMAT;
        status=campaign_clutter_registered(owner->state.handle,&slot,&model);if(status)return status;
        if(slot!=i || owner->state.model!=owner->attachment.model ||
           campaign_clutter_shared[model].resource!=&campaign_clutter_models[model].resource ||
           owner->uid!=campaign_clutter_records.items[i].uid)return RF_FORMAT;
        for(j=0;j<3;j++){
            /* The model's origin-centered bound encloses its mesh. Absolute
             * basis columns conservatively transform that enclosing cube,
             * including the small roundoff in authored rotation matrices. */
            double radius=(double)owner->attachment.radius*(fabs((double)owner->matrix[j])+
                fabs((double)owner->matrix[3+j])+fabs((double)owner->matrix[6+j]));
            double a=(double)owner->state.position[j]-radius,b=(double)owner->state.position[j]+radius;
            if(!isfinite(owner->body.state.bounds.minimum[j]) || !isfinite(owner->body.state.bounds.maximum[j]) ||
               owner->body.state.bounds.minimum[j]>owner->body.state.bounds.maximum[j])return RF_FORMAT;
            minimum[j]=a<owner->body.state.bounds.minimum[j]?a:owner->body.state.bounds.minimum[j];
            maximum[j]=b>owner->body.state.bounds.maximum[j]?b:owner->body.state.bounds.maximum[j];
        }
        for(j=0;j<query->count;j++){
            const rf_collision_body_sphere *sphere=query->spheres+j;
            rf_collision_model_part_query mesh_query={0};rf_collision_model_response_hit mesh_hit={0};
            rf_collision_ray_hit local;float start[3];uint32_t accepted;
            static const float stationary[3]={0};
            for(k=0;k<3;k++){
                volatile float rotated=(float)(((double)sphere->center[2]*query->matrix[2][k]+
                    (double)sphere->center[1]*query->matrix[1][k])+(double)sphere->center[0]*query->matrix[0][k]);
                start[k]=rotated+query->start[k];if(!isfinite(start[k]))return RF_FORMAT;
            }
            if(!scene_player_clutter_bounds(start,delta,sphere->radius,minimum,maximum,limit))continue;
            memcpy(mesh_query.input.start,start,12);memcpy(mesh_query.input.displacement,delta,12);
            memcpy(mesh_query.input.origin,owner->state.position,12);memcpy(mesh_query.input.matrix,owner->matrix,36);
            mesh_query.input.radius=sphere->radius;mesh_hit.time=limit;
            /* flags0 requests the nearest triangle, not model first-hit mode.
             * reset0 preserves the already-shortened nearest-time limit. */
            status=rf_scene_clutter_collision_query(owner->state.handle,&mesh_query,&mesh_hit,0,&accepted);if(status)return status;
            if(!accepted)continue;
            if(!isfinite(mesh_hit.time) || mesh_hit.time<0 || mesh_hit.time>limit)return RF_FORMAT;
            if(mesh_hit.time==limit)continue;
            local.fraction=mesh_hit.time;memcpy(local.point,mesh_hit.point,12);memcpy(local.normal,mesh_hit.normal,12);
            memset(&value,0,sizeof(value));
            status=rf_collision_mover_contact(&local,owner->state.position,(const float (*)[3])owner->matrix,
                stationary,owner->state.handle,UINT32_MAX,owner->material,0,mesh_hit.part,&value.contact);if(status)return status;
            value.solid=value.face=UINT32_MAX;value.sphere=j;value.hits=1;
            value.room=owner->state.first_word?owner->state.first_word-1:UINT32_MAX;
            limit=mesh_hit.time;any=1;if(!limit)goto done;
        }
    }
 done:
    if(any){*result=value;*matched=1;}
    if(replaced)*replaced=any;return RF_OK;
}
#endif
