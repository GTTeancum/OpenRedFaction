#ifndef RF_SCENE_TACTICAL_LOCAL_REGION_SAMPLE_INCLUDED
#define RF_SCENE_TACTICAL_LOCAL_REGION_SAMPLE_INCLUDED
/* Staged parent implementation, not wired into gameplay.
 * 4cecd0 / 4cee50 ground-mode projection. Caller supplies the first qualified
 * start region after existing search_prepare/connect_start. The graph remains
 * borrowed and unchanged. The private owner RNG is the approved adaptation.
 * No allocation, graph search, collision callback or scene mutation here.
 * Output and RNG are published together only after finite geometry succeeds.
 */
typedef struct scene_tactical_region_sample {
    uint32_t first,selected,count;
    float goal[3];
} scene_tactical_region_sample;

static int scene_tactical_sample_region(
    const rf_entity_navigation_reference *refs,uint32_t count,uint32_t first,
    float actor_radius,rf_random_state *random,scene_tactical_region_sample *out)
{
    scene_tactical_region_sample sample={0};rf_random_state next;
    const rf_entity_navigation_reference *start;const rf_entity_navigation_candidate *node;
    uint32_t i,index,eligible=0,draw,choice,radial,angular;float radius,distance;double angle;
    int status;
    if(!refs||!random||!out||!count||count>65536u||first>=count)return RF_RANGE;
    if(!isfinite(actor_radius)||actor_radius<0)return RF_FORMAT;
    start=refs+first;
    if(!start->candidate||start->candidate->rejected_035||
       (start->neighbor_count&&!start->neighbors)||start->neighbor_count>65536u)return RF_FORMAT;
    for(i=0;i<start->neighbor_count;i++) {
        index=start->neighbors[i];
        if(index>=count||!refs[index].candidate)return RF_FORMAT;
        if(!refs[index].candidate->rejected_035)++eligible;
    }
    next=*random;
    status=rf_random_next(&next,&draw);if(status)return status;
    choice=draw%(eligible+1u);index=first;
    /* Original 4ced65: the current region is the LAST choice, after eligible
     * neighbors in their authored order. Preserve duplicate adjacency entries. */
    if(choice!=eligible) {
        for(i=0;i<start->neighbor_count;i++) {
            uint32_t neighbor=start->neighbors[i];
            if(refs[neighbor].candidate->rejected_035)continue;
            if(!choice){index=neighbor;break;}
            --choice;
        }
        if(i==start->neighbor_count)return RF_FORMAT;
    }
    node=refs[index].candidate;
    for(i=0;i<3;i++)if(!isfinite(node->query_point[i])||!isfinite(node->position[i]))return RF_FORMAT;
    if(!isfinite(node->radius)||node->radius<actor_radius)return RF_FORMAT;
    radius=(float)((double)node->radius-actor_radius);
    if(!isfinite(radius))return RF_FORMAT;
    status=rf_random_next(&next,&radial);if(status)return status;
    status=rf_random_next(&next,&angular);if(status)return status;
    /* 504db0 uses exactly 1/32768 (589de8: 00000038), NOT 1/32767.
     * 4ceea7 stores the radius draw as float before the angle draw; the angle
     * is used unrounded by x87 sin/cos against float 40c90fdb. */
    distance=(float)((double)radial*(1.0/32768.0)*(double)radius);
    angle=(double)angular*(1.0/32768.0)*(double)0x1.921fb6p+2f;
    sample.first=first;sample.selected=index;
    /* Use the original choice, not node equality: an authored self-neighbor
     * follows the four-node branch just as any other adjacency entry. */
    sample.count=(draw%(eligible+1u)==eligible)?2u:4u;
    sample.goal[0]=(float)((double)node->position[0]+cos(angle)*(double)distance);
    sample.goal[1]=node->query_point[1];
    sample.goal[2]=(float)((double)node->position[2]+sin(angle)*(double)distance);
    for(i=0;i<3;i++)if(!isfinite(sample.goal[i]))return RF_FORMAT;
    *random=next;*out=sample;return RF_OK;
}
#endif
