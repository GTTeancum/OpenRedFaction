#ifndef RF_SCENE_CHECKPOINT_CLUTTER_SUPPORT_INC
#define RF_SCENE_CHECKPOINT_CLUTTER_SUPPORT_INC
/* Ordinary standing-player floor queries against an already prepared RFPC
 * candidate. Include after scene_world_restore.inc. Geometry is borrowed;
 * saved flags/health/pose come only from the candidate, never the live query
 * wrapper. No allocations, owner mutation, clearance exceptions or save data. */
typedef struct scene_checkpoint_clutter_support {
    const scene_world_restore_stage *world;
    uint32_t player_body_flags;
} scene_checkpoint_clutter_support;

static int scene_checkpoint_clutter_stationary(uint32_t slot,const rf_clutter_base_owner *owner,
    const rf_clutter_base_owner *authored)
{
    const rf_level_clutter *record=campaign_clutter_records.items+slot;
    const rf_physics_body_state *body=&owner->body.state;uint32_t i;
    if(owner->parent_byte||owner->parent_group!=1)return 0;
    for(i=0;i<campaign_group_control_child_count;i++)
        if(campaign_group_control_children[i].kind==SCENE_GROUP_CONTROL_CLUTTER&&
           campaign_group_control_children[i].source==slot)return 0;
    /* An attachment publishes the original owner, not the staged copy. Its
     * identity is useful here; its live hidden/retired flags are not. */
    if(campaign_attachment_nodes){
        const rf_attachment_node *node;
        if(slot>=campaign_attachment_count)return 0;
        node=campaign_attachment_nodes+slot;
        if(node->owner&&(node->owner!=&authored->state||node->flags!=&authored->state.flags||
           node->parent!=UINT32_MAX))return 0;
    }
    for(i=0;i<3;i++)
        if(!isfinite(owner->state.position[i])||owner->state.position[i]!=record->position[i]||
           body->position[i]!=owner->state.position[i]||body->next_position[i]!=owner->state.position[i]||
           body->velocity[i]!=0||body->vector_c8[i]!=0||body->mass_vector_d4[i]!=0)return 0;
    for(i=0;i<9;i++)
        if(!isfinite(owner->matrix[i])||owner->matrix[i]!=record->matrix[i/3][i%3]||
           body->orientation[i]!=owner->matrix[i]||body->next_orientation[i]!=owner->matrix[i])return 0;
    return 1;
}

/* Same finite-radius static-mesh trace, strict nearest rule and original
 * player/type4 admission as the ordinary live composer. RFPC rows are UID
 * sorted and compact: the prepared entry supplies their authored slot. */
static int scene_checkpoint_clutter_compose(void *context,const rf_collision_body_query *query,
    rf_geometry_body_hit *result,uint32_t *matched)
{
    const scene_checkpoint_clutter_support *c=context;const scene_world_restore_stage *world;
    rf_geometry_body_hit value={0};float delta[3],limit;uint32_t i,j,k,slot,any=0;int status;
    if(!c||!(world=c->world)||!world->clutter||!query||!result||!matched||*matched>1||
       (query->count&&!query->spheres)||query->count>64||!isfinite(query->limit)||
       query->limit<0||query->limit>1||world->clutter->count!=world->world.prop_count||
       (world->clutter->count&&(!world->candidate_props||!world->prop_views||!world->authored_prop_views))||
       (campaign_group_control_child_count&&!campaign_group_control_children))return RF_RANGE;
    /* Generation-qualified ownership check only. This helper does not read
     * live visibility/death state and does not substitute a current contact. */
    status=scene_clutter_checkpoint_targets_valid(world->clutter);if(status)return status;
    limit=query->limit;
    if(*matched){
        if(!isfinite(result->contact.fraction)||result->contact.fraction<0||result->contact.fraction>1)return RF_FORMAT;
        if(result->contact.fraction<limit)limit=result->contact.fraction;
    }
    if(!query->count||!limit||!world->clutter->count)return RF_OK;
    if(!campaign_clutter_records.items||!campaign_clutter_bodies||!campaign_clutter_model_slots||
       !campaign_clutter_shared||!campaign_clutter_models||!campaign_clutter_classes.items)return RF_FORMAT;
    for(i=0;i<3;i++){
        if(!isfinite(query->start[i])||!isfinite(query->end[i]))return RF_FORMAT;
        delta[i]=query->end[i]-query->start[i];if(!isfinite(delta[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(query->matrix[i][j]))return RF_FORMAT;
    }
    for(i=0;i<query->count;i++){
        if(!isfinite(query->spheres[i].radius)||query->spheres[i].radius<0)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(query->spheres[i].center[j]))return RF_FORMAT;
    }
    if(delta[0]==0&&delta[1]==0&&delta[2]==0)return RF_OK;
    /* Preserve authored placement order even though RFPC rows are UID sorted.
     * This bounded save-time lookup allocates nothing and keeps exact prop ties
     * consistent with the live composer. */
    for(slot=0;slot<campaign_clutter_records.count;slot++){
        const scene_clutter_restore_entry *entry;
        const rf_clutter_base_owner *owner,*authored;
        const rf_model_collision_resource *geometry;uint32_t model;
        double minimum[3],maximum[3];
        for(i=0;i<world->clutter->count&&world->clutter->entries[i].slot!=slot;i++){}
        if(i==world->clutter->count)continue;
        entry=world->clutter->entries+i;owner=world->candidate_props+i;
        if(world->prop_views[i]!=owner||
           !(authored=campaign_clutter_bodies[slot])||owner->uid!=entry->row.uid||
           owner->uid!=campaign_clutter_records.items[slot].uid||owner->uid!=authored->uid||
           owner->state.handle!=entry->handle||owner->state.handle!=authored->state.handle||
           owner->state.class_index<0||(uint32_t)owner->state.class_index>=campaign_clutter_classes.count||
           owner->state.class_index!=authored->state.class_index||
           owner->state.definition!=campaign_clutter_classes.items+owner->state.class_index||
           owner->state.definition!=authored->state.definition||
           owner->body.spheres.items!=authored->body.spheres.items||
           owner->body.spheres.count!=authored->body.spheres.count)return RF_FORMAT;
        /* A rebuilt group-child pose has no authored-static alias. A live
         * hidden/dead baseline remains a valid immutable geometry owner. */
        if(!world->authored_prop_views[i])continue;
        if(world->authored_prop_views[i]!=authored)return RF_FORMAT;
        if((owner->state.flags&(2u|0x4000u|0x40000u))||
           !(owner->state.definition->flags&4u)||(owner->state.definition->flags&2u)||
           !((c->player_body_flags|owner->body.state.flags)&0x20u)||
           !(owner->body.state.bounds.radius>.5f))continue;
        if(!scene_checkpoint_clutter_stationary(slot,owner,authored))continue;
        model=campaign_clutter_model_slots[slot];
        if(model>=campaign_clutter_model_count||
           owner->attachment.model!=(uint32_t)(uintptr_t)(campaign_clutter_shared+model)||
           owner->state.model!=owner->attachment.model||
           owner->attachment.model!=authored->attachment.model||
           campaign_clutter_shared[model].resource!=&campaign_clutter_models[model].resource||
           !isfinite(owner->body.state.bounds.radius)||!isfinite(owner->attachment.radius)||
           owner->attachment.radius<0||owner->material>=10||owner->material!=owner->state.definition->material)return RF_FORMAT;
        geometry=&campaign_clutter_models[model].collision;
        if(geometry->part_count<=0||!geometry->parts)return RF_FORMAT;
        for(j=0;j<3;j++){
            double radius=(double)owner->attachment.radius*(fabs((double)owner->matrix[j])+
                fabs((double)owner->matrix[3+j])+fabs((double)owner->matrix[6+j]));
            double a=(double)owner->state.position[j]-radius,b=(double)owner->state.position[j]+radius;
            if(!isfinite(owner->body.state.bounds.minimum[j])||!isfinite(owner->body.state.bounds.maximum[j])||
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
            /* flags0/reset0 preserve nearest ordering and the existing limit.
             * No rf_scene_clutter_collision_query: that gates on live flags. */
            accepted=rf_collision_model_trace(geometry->parts,&geometry->part_count,&mesh_query,&mesh_hit,0);
            if(!accepted)continue;
            if(!isfinite(mesh_hit.time)||mesh_hit.time<0||mesh_hit.time>limit)return RF_FORMAT;
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
    if(any){*result=value;*matched=1;}return RF_OK;
}
#endif
