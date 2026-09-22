#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_npc_checkpoint_pair_fit.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC pair fit line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {rf_checkpoint_placement candidate[3];uint32_t retired[3];} fixture;
static int read_candidate(void *context,uint32_t index,rf_checkpoint_placement *out,uint32_t *retired)
{fixture *f=context;if(index>=3)return RF_RANGE;*retired=f->retired[index];if(!*retired)*out=f->candidate[index];return RF_OK;}
int main(void)
{
    fixture f={0},before;rf_physics_sphere spheres[2]={{0}};unsigned i,j;
    spheres[0].radius=spheres[1].radius=1;spheres[1].center[0]=2;
    for(i=0;i<3;i++){f.candidate[i].spheres=spheres;f.candidate[i].count=1;for(j=0;j<3;j++)f.candidate[i].basis[j*3+j]=1;}
    f.candidate[1].position[0]=2;f.candidate[2].position[0]=4;before=f;
    CHECK(!scene_npc_checkpoint_pair_fit(3,read_candidate,&f));CHECK(!memcmp(&before,&f,sizeof(f)));
    f.candidate[1].position[0]=1.999f;CHECK(!scene_npc_checkpoint_pair_fit(2,read_candidate,&f));
    f.candidate[1].position[0]=1.99f;before=f;
    CHECK(scene_npc_checkpoint_pair_fit(2,read_candidate,&f)==RF_NOT_FOUND&&!memcmp(&before,&f,sizeof(f)));
    f.retired[1]=1;f.candidate[1].spheres=NULL;CHECK(!scene_npc_checkpoint_pair_fit(3,read_candidate,&f));
    f.retired[1]=0;f.candidate[1].spheres=spheres;f.candidate[1].position[0]=0;f.candidate[1].position[1]=3;
    f.candidate[0].count=2;memset(f.candidate[0].basis,0,36);
    f.candidate[0].basis[1]=1;f.candidate[0].basis[3]=-1;f.candidate[0].basis[8]=1;
    CHECK(scene_npc_checkpoint_pair_fit(2,read_candidate,&f)==RF_NOT_FOUND); /* Rotated offset sphere at Y2. */
    f.retired[0]=1;CHECK(!scene_npc_checkpoint_pair_fit(2,read_candidate,&f));
    f.candidate[1].position[0]=NAN;CHECK(scene_npc_checkpoint_pair_fit(2,read_candidate,&f)==RF_FORMAT);
    f.retired[1]=1;CHECK(!scene_npc_checkpoint_pair_fit(2,read_candidate,&f));
    f.retired[0]=f.retired[1]=0;
    f.candidate[0].count=0;f.candidate[0].spheres=NULL;
    f.candidate[1].position[0]=f.candidate[1].position[1]=0;
    CHECK(!scene_npc_checkpoint_pair_fit(2,read_candidate,&f)); /* Empty set overlaps nothing. */
    f.candidate[0].position[0]=NAN;
    CHECK(scene_npc_checkpoint_pair_fit(2,read_candidate,&f)==RF_FORMAT);
    f.candidate[0].position[0]=0;f.candidate[0].basis[8]=0;
    CHECK(scene_npc_checkpoint_pair_fit(2,read_candidate,&f)==RF_FORMAT);
    f.candidate[0].basis[8]=1;f.candidate[0].spheres=spheres;
    CHECK(scene_npc_checkpoint_pair_fit(2,read_candidate,&f)==RF_RANGE);
    CHECK(scene_npc_checkpoint_pair_fit(RF_NPC_CHECKPOINT_MAX_COUNT+1,read_candidate,&f)==RF_RANGE);
    puts("Candidate NPC pair fit: tolerance, staged translation/rotation, sphere unions, retired exclusion and immutable inputs passed");return 0;
}
