#include "rf/collision.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <string.h>
typedef struct campaign_npc_body {int view;struct {void *view;uint32_t handle;} registration;} campaign_npc_body;
typedef struct campaign_model_owner {float position[3],basis[9];} campaign_model_owner;
typedef struct scene_npc_shield_candidate {uint32_t slot,handle;float limit;rf_collision_model_response_hit hit;} scene_npc_shield_candidate;
static campaign_npc_body npc,*campaign_npc_bodies=&npc;static uint32_t campaign_npc_body_count=1,campaign_model_owner_count=1;
static campaign_model_owner model,*campaign_model_owners=&model;static int campaign_entities,answer,status_value;static uint32_t calls;
static void *rf_entity_lookup(void *set,int32_t handle){(void)set;return (uint32_t)handle==npc.registration.handle?&npc.view:NULL;}
static int rf_scene_model_collision_query(uint32_t slot,rf_collision_model_part_query *query,rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted)
{
    ++calls;assert(slot==0 && reset==0 && query->input.flags==1 && query->input.radius==0);
    assert(hit->time==.6f);assert(!memcmp(query->input.origin,model.position,12));assert(!memcmp(query->input.matrix,model.basis,36));
    assert(query->input.start[0]==12 && query->input.displacement[0]==-4);
    assert(query->input.start[1]==3 && query->input.displacement[1]==0);
    if(status_value)return status_value;*accepted=answer;if(answer)hit->time=.4f;return RF_OK;
}
#include "../src/diagnostic/scene_riot_shield_body_order.inc"
int main(void)
{
    scene_npc_shield_candidate c={0};float start[3]={12,3,4},end[3]={8,3,4};uint32_t before=99;
    model.position[0]=10;model.position[1]=3;model.position[2]=4;
    model.basis[2]=-1;model.basis[4]=1;model.basis[6]=1;
    npc.registration.view=&npc.view;npc.registration.handle=c.handle=7;c.hit.time=.6f;
    assert(scene_npc_shield_body_before(&c,start,end,&before)==RF_OK && !before && calls==1);
    answer=1;assert(scene_npc_shield_body_before(&c,start,end,&before)==RF_OK && before && calls==2);
    status_value=RF_NOT_FOUND;before=99;
    assert(scene_npc_shield_body_before(&c,start,end,&before)==RF_NOT_FOUND && before==99);
    status_value=0;c.handle=8;
    assert(scene_npc_shield_body_before(&c,start,end,&before)==RF_NOT_FOUND && calls==3);
    return 0;
}
