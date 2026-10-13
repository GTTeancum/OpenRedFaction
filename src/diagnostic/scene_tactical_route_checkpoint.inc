#ifndef RF_SCENE_TACTICAL_ROUTE_CHECKPOINT_INCLUDED
#define RF_SCENE_TACTICAL_ROUTE_CHECKPOINT_INCLUDED
/* Read-only geometry join. Do not replay sampling or compare clearance against
 * the target's later position. Prepared query coordinates are derived locally,
 * never taken from shared search scratch last used by a different actor. */
static int scene_tactical_saved_goal_in_region(const rf_entity_navigation_candidate *node,
    float radius,float height,const float goal[3],uint32_t *inside)
{
    double x,z,limit,error;float y;
    if(!node||!goal||!inside||!isfinite(radius)||radius<0||!isfinite(height)||height<0)return RF_RANGE;
    if(!isfinite(node->radius)||!isfinite(node->height)||node->radius<0||node->height<0)return RF_FORMAT;
    for(uint32_t j=0;j<3;j++)if(!isfinite(node->position[j])||!isfinite(goal[j]))return RF_FORMAT;
    *inside=0;
    if(!rf_entity_navigation_candidate_allowed(radius,height,1,node->radius,node->height,node->word_040))return RF_OK;
    y=(float)(((double)node->position[1]-(double)node->height*.5)+height);
    if(!isfinite(y)||goal[1]!=y)return RF_OK;
    limit=(float)((double)node->radius-radius);
    x=(double)goal[0]-node->position[0];z=(double)goal[2]-node->position[2];
    /* Sampling writes final float coordinates. Bound that rounding rather than
     * rejecting valid circle-boundary samples after a save round-trip. */
    error=4.0*1.1920928955078125e-7*(fabs((double)node->position[0])+fabs((double)node->position[2])+limit+1.0);
    limit+=error;*inside=x*x+z*z<=limit*limit;return RF_OK;
}
static int scene_tactical_saved_route(const rf_npc_checkpoint_record *row,
    const rf_entity_navigation_reference *refs,uint32_t count,float radius,float height)
{
    const rf_npc_checkpoint_move *m;uint32_t first,last,inside;int status;
    if(!row||(count&&!refs)||count>65536u)return RF_RANGE;
    if(!row->tactical.present||row->tactical.phase!=SCENE_TACTICAL_RELOCATING)return RF_OK;
    m=&row->move;
    if(!m->active||m->follow!=2||m->event||m->path_count||m->retry||
       row->route.origin||(row->route.flags&126u)||
       (m->retained_count!=2&&m->retained_count!=4)||!m->route_index||
       m->route_index>=m->retained_count||m->retained_nodes[0]!=UINT32_MAX||
       m->retained_nodes[m->retained_count-1]!=UINT32_MAX-1||memcmp(m->target,m->route_goal,12))return RF_FORMAT;
    if(m->retained_count==2){
        /* Two-node routes never serialized a graph node. Admit membership in
         * a compatible authored region, without inventing or replaying a
         * selection-time region identity that this wire does not contain. */
        for(uint32_t i=0;i<count;i++){
            status=scene_tactical_saved_goal_in_region(refs[i].candidate,radius,height,m->route_goal,&inside);if(status)return status;
            if(inside)return RF_OK;
        }
        return RF_FORMAT;
    }
    first=m->retained_nodes[1];last=m->retained_nodes[2];
    if(!first||first>count||!last||last>count)return RF_FORMAT;--first;--last;
    if(!refs[first].candidate||!refs[last].candidate||
       (refs[first].neighbor_count&&!refs[first].neighbors)||refs[first].neighbor_count>65536u)return RF_FORMAT;
    if(!isfinite(refs[first].candidate->radius)||!isfinite(refs[first].candidate->height)||
       !rf_entity_navigation_candidate_allowed(radius,height,1,refs[first].candidate->radius,
           refs[first].candidate->height,refs[first].candidate->word_040))return RF_FORMAT;
    for(uint32_t i=0;i<refs[first].neighbor_count;i++)if(refs[first].neighbors[i]==last){
        status=scene_tactical_saved_goal_in_region(refs[last].candidate,radius,height,m->route_goal,&inside);
        return status?status:inside?RF_OK:RF_FORMAT;
    }
    return RF_FORMAT;
}
#endif
