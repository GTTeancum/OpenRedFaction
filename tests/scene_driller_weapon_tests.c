#include <stdio.h>
#include "../src/diagnostic/scene_driller_weapon.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"driller weapon line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint32_t mode,traces,damages,cuts;float damage,requested_radius,center[3],basis[9];char model[64];} fixture;
static int trace(void *context,uint32_t source,const float *start,const float *end,float radius,
    rf_weapon_flight_contact *out,uint32_t *matched)
{
    fixture *f=context;uint32_t i;
    if(source!=123||fabsf(radius-.2f)>.0001f||fabsf(end[2]-start[2]-3.5f)>.0001f)return RF_FORMAT;
    ++f->traces;*matched=f->mode!=0&&(f->mode!=3||(f->traces&1));
    if(*matched){memset(out,0,sizeof(*out));out->object=f->mode==2?42:UINT32_MAX;
        for(i=0;i<3;i++)out->hit.point[i]=(start[i]+end[i])*.5f;}
    return RF_OK;
}
static int damage(void *context,const rf_weapon_flight_contact *hit,float amount,uint32_t source)
{fixture *f=context;if(hit->object!=42||source!=123)return RF_FORMAT;++f->damages;f->damage+=amount;return RF_OK;}
static int cut(void *context,const float *center,const float *basis,const char *model,float requested_radius,
    uint32_t flags,uint32_t source,uint32_t *accepted)
{fixture *f=context;if(flags!=0x3e||source!=123||memcmp(basis,f->basis,sizeof(f->basis)))return RF_FORMAT;
 ++f->cuts;f->requested_radius=requested_radius;memcpy(f->center,center,12);strcpy(f->model,model);*accepted=1;return RF_OK;}
int main(void)
{
    rf_vpp meshes={0},tables={0};rf_model_file model={0};rf_static_model_tags tags={0};
    scene_driller_weapon state;fixture f={0};scene_driller_weapon_backend backend={&f,trace,damage,cut};
    float position[3]={0},basis[9]={1,0,0,0,1,0,0,0,1};uint32_t i,prior;
    memcpy(f.basis,basis,sizeof(basis));
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    CHECK(!rf_model_file_open(&model,&meshes,"Driller01.v3m"));
    CHECK(!rf_static_model_tags_open(&model,65536,&tags));
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
    CHECK(!scene_driller_weapon_open(&tables,&tags,2*1024*1024,&state));
    CHECK(state.damage==30&&state.fire_seconds==.5f&&state.start_seconds==1&&state.radius==.2f);
    CHECK(state.tags[0]==26&&state.tags[1]==27);
    f.mode=1;
    for(i=0;i<9;i++)CHECK(!scene_driller_weapon_tick(&state,&tags,position,basis,123,1,.1f,&backend));
    CHECK(f.traces==0);
    for(i=0;i<10;i++)CHECK(!scene_driller_weapon_tick(&state,&tags,position,basis,123,1,.1f,&backend));
    CHECK(f.cuts==1&&state.accepted_cuts==1&&f.requested_radius==20&&!strcmp(f.model,"bit_driller_double.v3d"));
    CHECK(fabsf(f.center[1]-(tags.items[26].position[1]+.78f))<.0001f);
    CHECK(!scene_driller_weapon_tick(&state,&tags,position,basis,123,0,.1f,&backend));
    CHECK(!state.contact_active&&state.accepted_cuts==1);
    f.mode=2;prior=f.damages;
    for(i=0;i<10;i++)CHECK(!scene_driller_weapon_tick(&state,&tags,position,basis,123,1,.1f,&backend));
    CHECK(f.damages==prior+1&&f.damage==30); /* Both bits, one victim, one dose. */
    /* Preserve full host roll, not just its unchanged forward vector. */
    basis[0]=0;basis[1]=1;basis[3]=-1;basis[4]=0;memcpy(f.basis,basis,sizeof(basis));
    f.mode=3;prior=f.cuts;
    for(i=0;i<10;i++)CHECK(!scene_driller_weapon_tick(&state,&tags,position,basis,123,1,.1f,&backend));
    CHECK(f.cuts==prior+1&&f.requested_radius==10&&!strcmp(f.model,"bit_driller_single.v3d"));
    f.mode=1;state.accepted_cuts=25;prior=f.cuts;
    for(i=0;i<20;i++)CHECK(!scene_driller_weapon_tick(&state,&tags,position,basis,123,1,.1f,&backend));
    CHECK(f.cuts==prior&&state.accepted_cuts==25);
    rf_static_model_tags_close(&tags);rf_vpp_close(&meshes);rf_vpp_close(&tables);
    puts("installed Drill fields/tags, held warmup, two-bit cut, release, victim dedup and cap pass; scene collision/CSG callbacks are fixtures");return 0;
}
