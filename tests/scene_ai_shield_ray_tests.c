/* Actual selection/commit adapter with controlled triangle-query services.
 * Physical triangle skinning/contact are exercised by their existing tests;
 * this checks the missing sphere-miss path and ordering, not visual fidelity. */
#include "rf/entity.h"
#include "rf/collision.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"AI shield ray line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct scene_npc_shield_candidate {uint32_t slot,handle;float limit;rf_collision_model_response_hit hit;} scene_npc_shield_candidate;
static struct {void *owners;} scene_npc_shields;
static uint32_t found,before,commits;static float shield_time;
static int scene_npc_shield_query(uint32_t slot,const float *start,const float *end,float limit,scene_npc_shield_candidate *out,uint32_t *hit)
{(void)start;(void)end;if(limit!=1)return RF_RANGE;*hit=found;if(found){memset(out,0,sizeof(*out));out->slot=slot;out->handle=42;out->limit=limit;out->hit.time=shield_time;}return RF_OK;}
static int scene_npc_shield_body_before(const scene_npc_shield_candidate *s,const float *a,const float *b,uint32_t *hit)
{(void)s;(void)a;(void)b;*hit=before;return RF_OK;}
static int scene_npc_shield_commit(const scene_npc_shield_candidate *s,const float *a,const float *b,float damage,int32_t kind,uint32_t *accepted,uint32_t *broken)
{(void)a;(void)b;if(s->limit!=1 || s->handle!=42 || damage!=10 || kind!=0)return RF_RANGE;++commits;*accepted=*broken=1;return RF_OK;}
#include "../src/diagnostic/scene_ai_shield_ray.inc"
int main(void)
{
    float start[3]={0},end[3]={0,0,10};scene_ai_shield_ray_selection s;rf_damage_request request={10,123,0,0,UINT32_MAX,0};uint32_t accepted,broken;
    scene_npc_shields.owners=&s;found=1;shield_time=.4f;
    CHECK(!scene_ai_shield_ray_select(3,start,end,0,1,&s) && s.kind==SCENE_AI_SHIELD_RAY_SHIELD && s.fraction==.4f && !commits);
    CHECK(!scene_ai_shield_ray_commit(&s,start,end,&request,&accepted,&broken) && accepted && broken && commits==1 && request.source==123);
    /* A coarse sphere may start earlier but actual exposed shield wins. */
    CHECK(!scene_ai_shield_ray_select(3,start,end,1,.2f,&s) && s.kind==SCENE_AI_SHIELD_RAY_SHIELD && s.fraction==.4f);
    before=1;CHECK(!scene_ai_shield_ray_select(3,start,end,1,.2f,&s) && s.kind==SCENE_AI_SHIELD_RAY_BODY && s.fraction==.2f);
    CHECK(!scene_ai_shield_ray_commit(&s,start,end,&request,&accepted,&broken) && !accepted && !broken && commits==1);
    CHECK(!scene_ai_shield_ray_select(3,start,end,0,1,&s) && s.kind==SCENE_AI_SHIELD_RAY_MISS);
    before=found=0;CHECK(!scene_ai_shield_ray_select(3,start,end,1,.3f,&s) && s.kind==SCENE_AI_SHIELD_RAY_BODY && s.fraction==.3f);
    scene_npc_shields.owners=NULL;CHECK(!scene_ai_shield_ray_select(3,start,end,0,1,&s) && s.kind==SCENE_AI_SHIELD_RAY_MISS);
    puts("NPC shield-only rays, exact body veto, cover endpoint and deferred break commit passed");return 0;
}
