/* Port translational preflight. Query only: it never publishes a mover/body
 * pose. Controller timing/arrival and atomic group commit must admit its result
 * before this can be used by gameplay. Angular motion remains separate. */
static int scene_fragment_push_fraction(const scene_detached_query_context *c,
    const rf_physics_body_state *body,uint32_t mover,const float delta[3],float *fraction)
{
    rf_physics_body_state relative;rf_group_attached_pose pose;rf_geometry_collision_movers one;
    rf_geometry_body_hit contact={0};float approach=0,depth,clearance,value;uint32_t k,found=0;int status;
    if(!c || !c->scene || !c->mesh || !body || !delta || !fraction || mover>=campaign_movers.count ||
       !campaign_movers.poses || !campaign_movers.views || !campaign_movers.owned)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(delta[k]))return RF_RANGE;
    pose=campaign_movers.poses[mover];relative=*body;
    if(pose.flags&0x40000u){*fraction=1;return RF_OK;}
    for(k=0;k<3;k++) {
        pose.position[k]+=delta[k];pose.minimum[k]+=delta[k];pose.maximum[k]+=delta[k];
        relative.position[k]+=delta[k];relative.next_position[k]=body->position[k];
        if(!isfinite(pose.position[k]) || !isfinite(pose.minimum[k]) || !isfinite(pose.maximum[k]) ||
           !isfinite(relative.position[k]))return RF_RANGE;
    }
    memcpy(relative.next_orientation,body->orientation,36);
    one=campaign_movers;one.count=1;one.poses=&pose;one.views+=mover;one.owned+=mover;
    status=scene_fragment_shape_mover_sweep(c->mesh,&relative,&one,1,&contact,&found);if(status)return status;
    if(!found){*fraction=1;return RF_OK;}
    for(k=0;k<3;k++)approach+=delta[k]*contact.contact.normal[k];
    if(approach<=0){*fraction=1;return RF_OK;}
    depth=approach*(1-contact.contact.fraction);
    status=scene_detached_push_clearance(c,body,mover,contact.contact.normal,depth,&clearance);if(status)return status;
    value=clearance>=depth?1:contact.contact.fraction+clearance/approach;
    if(!isfinite(value) || value<0 || value>1)return RF_RANGE;
    *fraction=value;return RF_OK;
}
/* Return the strictest admission across the same retained source ownership
 * used by live fragment physics. All outputs publish only after every query. */
static inline int scene_fragment_mover_admit(scene_stream *s,uint32_t mover,const float delta[3],float *fraction)
{
    uint32_t source,b,i,count;float value=1;int status;
    if(!s || !delta || !fraction || mover>=campaign_movers.count || s->terrain_source_count>4 ||
       (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(delta[i]))return RF_RANGE;
    count=s->terrain_source_count?s->terrain_source_count:1;
    for(source=0;source<count;source++) {
        rf_geomod_piece_registry *registry=scene_detached_source_registry(s,source);
        if(!registry)continue;
        for(b=0;b<rf_geomod_piece_registry_count(registry);b++) {
            rf_geomod_piece_batch *batch;
            status=rf_geomod_piece_registry_get(registry,b,&batch);if(status)return status;
            for(i=0;i<rf_geomod_piece_batch_count(batch);i++) {
                rf_geomod_owned_piece piece;rf_physics_body *body;scene_detached_query_context query={0};float allowed;
                if(!rf_geomod_piece_batch_alive(batch,i))continue;
                status=rf_geomod_piece_batch_get(batch,i,&piece,&body);if(status)return status;
                query.scene=s;query.mesh=&piece.mesh;query.spheres=&body->spheres;
                status=scene_fragment_push_fraction(&query,&body->state,mover,delta,&allowed);if(status)return status;
                if(allowed<value)value=allowed;
            }
        }
    }
    *fraction=value;return RF_OK;
}
