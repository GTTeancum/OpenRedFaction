/* Pure production staging + real codec/inertia math. World/owner callback
 * decisions controlled here; live water/geometry is covered by motion tests. */
#include <stdio.h>
#define RF_SCENE_DRILLER_CHECKPOINT_MATH_ONLY
#include "../src/diagnostic/scene_driller_checkpoint_adapter.inc"
#include "../src/diagnostic/scene_submarine_checkpoint.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sub checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct admission {uint32_t owner,hull,player,calls;int error;} admission;
static int owner(void *p,const rf_submarine_checkpoint *r,uint32_t *ok)
{admission *a=p;(void)r;a->calls|=1;*ok=a->owner;return a->error;}
static int hull(void *p,const scene_submarine_checkpoint_candidate *c,uint32_t *ok)
{admission *a=p;(void)c;a->calls|=2;*ok=a->hull;return 0;}
static int player(void *p,const scene_submarine_checkpoint_candidate *c,uint32_t *ok)
{admission *a=p;(void)c;a->calls|=4;*ok=a->player;return 0;}
int main(void)
{
    rf_vehicle_rigid_state state={0},before;scene_submarine_checkpoint_live live={0};
    rf_submarine_checkpoint record,decoded,saved;unsigned char bytes[RF_SUBMARINE_CHECKPOINT_BYTES];
    scene_submarine_checkpoint_candidate candidate,sentinel;admission a={1,1,1,0,0};
    scene_submarine_checkpoint_backend backend={&a,owner,hull,player};
    state.orientation[0]=state.orientation[4]=state.orientation[8]=1;
    state.inverse_inertia[0]=.5f;state.inverse_inertia[4]=.25f;state.inverse_inertia[8]=.125f;
    state.position[0]=-25;state.position[1]=-15;state.velocity[2]=3;state.momentum[1]=8;
    state.force[0]=12;state.torque[1]=9;before=state;
    live.physics=&state;live.health=700;live.alive=live.owner_valid=1;live.torpedo_reserve=17;live.torpedo_cooldown=1.25f;
    CHECK(!scene_submarine_checkpoint_capture_state(&live,&record));saved=record;
    CHECK(record.vehicle.angular_velocity[1]==2 && record.torpedo_reserve==17 && record.torpedo_cooldown==1.25f);
    CHECK(!rf_submarine_checkpoint_encode(&record,bytes,sizeof(bytes)));
    CHECK(!rf_submarine_checkpoint_decode(bytes,sizeof(bytes),&decoded));
    CHECK(!scene_submarine_checkpoint_stage(&live,&decoded,&backend,&candidate));
    CHECK(a.calls==7 && candidate.vehicle.physics.momentum[1]==8 && candidate.vehicle.health==700);
    CHECK(candidate.torpedo_reserve==17 && candidate.torpedo_cooldown==1.25f);
    CHECK(!candidate.vehicle.physics.force[0] && !candidate.vehicle.physics.torque[1]);
    CHECK(!memcmp(&state,&before,sizeof(state)) && !memcmp(&record,&saved,sizeof(record)));
    record.vehicle.player_occupied=1;
    CHECK(!scene_submarine_checkpoint_stage(&live,&record,&backend,&candidate) && candidate.vehicle.player_occupied==1);
    memset(&sentinel,0xa5,sizeof(sentinel));candidate=sentinel;
    a.hull=0;CHECK(scene_submarine_checkpoint_stage(&live,&record,&backend,&candidate)==RF_NOT_FOUND);
    CHECK(!memcmp(&candidate,&sentinel,sizeof(candidate)));a.hull=1;a.player=0;
    CHECK(scene_submarine_checkpoint_stage(&live,&record,&backend,&candidate)==RF_NOT_FOUND);
    CHECK(!memcmp(&candidate,&sentinel,sizeof(candidate)));a.player=1;a.owner=0;
    CHECK(scene_submarine_checkpoint_stage(&live,&record,&backend,&candidate)==RF_NOT_FOUND);a.owner=1;
    live.active_torpedoes=1;
    CHECK(scene_submarine_checkpoint_capture_state(&live,&decoded)==RF_NOT_FOUND);
    CHECK(scene_submarine_checkpoint_stage(&live,&record,&backend,&candidate)==RF_NOT_FOUND);
    live.active_torpedoes=0;live.occupied=1;
    CHECK(scene_submarine_checkpoint_stage(&live,&record,&backend,&candidate)==RF_NOT_FOUND);
    live.occupied=0;a.error=RF_IO;
    CHECK(scene_submarine_checkpoint_stage(&live,&record,&backend,&candidate)==RF_IO);
    CHECK(!memcmp(&candidate,&sentinel,sizeof(candidate)));
    puts("PASS submarine cooldown/ammo/pose staging, real angular momentum and water/player/owner rejection");return 0;
}
