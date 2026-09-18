#include <stdio.h>
#include "../src/diagnostic/scene_submarine_weapon.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sub weapon line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct probe {uint32_t wet,hit,steers;} probe;
static int water(void *p,const float at[3],uint32_t *wet){(void)at;*wet=((probe*)p)->wet;return RF_OK;}
static int sweep(void *p,const float start[3],const float delta[3],float radius,rf_weapon_flight_contact *contact,uint32_t *matched)
{
    uint32_t i;(void)radius;memset(contact,0,sizeof(*contact));contact->hit.fraction=.5f;
    /* Flight core requires a physical unit contact normal, even for a mock. */
    contact->hit.normal[0]=-1;
    for(i=0;i<3;i++)contact->hit.point[i]=start[i]+delta[i]*.5f;
    *matched=((probe*)p)->hit;return RF_OK;
}
static int steer(void *p,const scene_submarine_weapon_definition *d,const rf_weapon_flight *f,float dt,float out[3],uint32_t *changed)
{(void)d;(void)f;(void)dt;((probe*)p)->steers++;out[0]=1;out[1]=out[2]=0;*changed=1;return RF_OK;}
int main(void)
{
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *model=NULL;
    scene_submarine_weapon_state state;rf_static_model_tags tags={0};rf_model_file *file=malloc(sizeof(*file));
    float pos[3]={0},basis[9]={1,0,0,0,1,0,0,0,1},muzzle[12];uint32_t fired,i;probe p={0};rf_weapon_flight_event event;char path[128];
    CHECK(file);CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));CHECK(!rf_model_file_open(file,&meshes,"Sub_Mini01.v3m"));
    CHECK(!rf_static_model_tags_open(file,65536,&tags));free(file);
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!scene_submarine_weapon_open(&tables,&tags,"primary_1",2*1024*1024,&state));rf_vpp_close(&tables);
    CHECK(!rf_static_model_tag_place(&tags,state.muzzle,basis,pos,muzzle));
    CHECK(state.definition.capacity==20 && state.definition.damage==200 && state.definition.speed==7 && state.definition.lifetime==10);
    CHECK(state.definition.blast==5 && state.definition.crater==5 && state.definition.turn_seconds==12 && state.definition.view_degrees==110);
    state.reserve=20;CHECK(!scene_submarine_weapon_fire(&state,&tags,pos,basis,NULL,1,2,1,1,.01f,water,&p,&fired));CHECK(!fired && state.reserve==20);
    p.wet=1;CHECK(!scene_submarine_weapon_fire(&state,&tags,pos,basis,NULL,1,2,1,1,.01f,water,&p,&fired));CHECK(fired && state.reserve==19 && !memcmp(state.rounds[0].flight.position,muzzle+9,12));
    CHECK(!scene_submarine_weapon_fire(&state,&tags,pos,basis,NULL,1,2,1,1,.01f,water,&p,&fired));CHECK(!fired && state.reserve==19);
    CHECK(!scene_submarine_weapon_step(state.rounds,&state.definition,.05f,sweep,&p,steer,&event));CHECK(!p.steers && event.kind==0);
    state.rounds[0].flight.remaining=9.8;CHECK(!scene_submarine_weapon_step(state.rounds,&state.definition,.05f,sweep,&p,steer,&event));CHECK(p.steers==1 && state.rounds[0].flight.velocity[0]==7);
    p.hit=1;{int status=scene_submarine_weapon_step(state.rounds,&state.definition,.05f,sweep,&p,NULL,&event);
        if(status)fprintf(stderr,"TORPEDO_IMPACT_STEP status=%d\n",status);CHECK(!status);}
    CHECK(event.kind==1 && !state.rounds[0].flight.active);
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_submarine_weapon_model_open(&meshes,maps,4,512*1024,&model));
    printf("TORPEDO_MODEL resident=%u peak=%u parts=%u textures=%u\n",model->resident_bytes,model->peak_bytes,model->render.part_count,model->materials.textures.count);
    scene_driller_resources_close(&model);rf_static_model_tags_close(&tags);rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    puts("PASS authored torpedo config/model, wet launch, finite reserve/cadence, swept impact, optional homing seam");return 0;
}
