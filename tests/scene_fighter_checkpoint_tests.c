#include <stdio.h>
#define RF_SCENE_DRILLER_CHECKPOINT_MATH_ONLY
#include "../src/diagnostic/scene_driller_checkpoint_adapter.inc"
#include "../src/diagnostic/scene_fighter_checkpoint.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"fighter checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct admission {uint32_t owner,hull,player,calls;} admission;
static int owner(void *p,const scene_fighter_checkpoint_record *r,uint32_t *ok)
{admission *a=p;a->calls|=1;*ok=a->owner && r->class_id==5;return RF_OK;}
static int hull(void *p,const scene_fighter_checkpoint_candidate *c,uint32_t *ok)
{admission *a=p;(void)c;a->calls|=2;*ok=a->hull;return RF_OK;}
static int player(void *p,const scene_fighter_checkpoint_candidate *c,uint32_t *ok)
{admission *a=p;(void)c;a->calls|=4;*ok=a->player;return RF_OK;}
int main(void)
{
    rf_vehicle_rigid_state physics={0},before;scene_fighter_weapon_state weapon={0},weapon_before;
    scene_fighter_checkpoint_live live={0};scene_fighter_checkpoint_record record,bad,saved;
    scene_fighter_checkpoint_candidate staged,sentinel;admission a={1,1,1,0};
    scene_fighter_checkpoint_backend backend={&a,owner,hull,player};uint32_t i;
    physics.orientation[0]=physics.orientation[4]=physics.orientation[8]=1;
    physics.inverse_inertia[0]=.5f;physics.inverse_inertia[4]=.25f;physics.inverse_inertia[8]=.125f;
    physics.position[1]=20;physics.velocity[0]=4;physics.velocity[1]=2;physics.momentum[1]=8;
    physics.force[0]=10;physics.torque[1]=5;before=physics;
    weapon.primary_reserve=870;weapon.rocket_reserve=19;
    weapon.primary_fire.cooldown=-1.f/60;weapon.rocket_fire.cooldown=2.25f;
    weapon.primary_fire.shots=30;weapon.rocket_fire.shots=1;weapon_before=weapon;
    live.physics=&physics;live.weapon=&weapon;live.health=900;live.alive=live.owner_valid=1;
    CHECK(!scene_fighter_checkpoint_capture_state(&live,&record));saved=record;
    CHECK(record.class_id==5 && record.vehicle.angular_velocity[1]==2);
    CHECK(!scene_fighter_checkpoint_stage(&live,&record,&backend,&staged));
    CHECK(a.calls==7 && staged.vehicle.physics.momentum[1]==8 && staged.vehicle.physics.velocity[1]==2);
    CHECK(staged.primary_reserve==870 && staged.rocket_reserve==19 && staged.rocket_fire.cooldown==2.25f);
    CHECK(staged.primary_fire.cooldown==-1.f/60 && staged.primary_fire.shots==30);
    CHECK(!staged.vehicle.physics.force[0] && !staged.vehicle.physics.torque[1]);
    CHECK(!memcmp(&physics,&before,sizeof(before)) && !memcmp(&weapon,&weapon_before,sizeof(weapon)));
    record.vehicle.player_occupied=1;
    CHECK(!scene_fighter_checkpoint_stage(&live,&record,&backend,&staged) && staged.vehicle.player_occupied);
    memset(&sentinel,0xa5,sizeof(sentinel));
    for(i=0;i<3;i++){
        staged=sentinel;a.owner=i!=0;a.hull=i!=1;a.player=i!=2;
        CHECK(scene_fighter_checkpoint_stage(&live,&record,&backend,&staged)==RF_NOT_FOUND);
        CHECK(!memcmp(&staged,&sentinel,sizeof(staged)));
    }
    a.owner=a.hull=a.player=1;
    for(i=0;i<4;i++){
        weapon=weapon_before;live.occupied=0;
        if(i==0)weapon.bullets[47].flight.active=1;
        if(i==1)weapon.rockets[3].flight.active=1;
        if(i==2)weapon.primary_fire.held=1;
        if(i==3)weapon.rocket_fire.warmup=.01f;
        bad=saved;CHECK(scene_fighter_checkpoint_capture_state(&live,&bad)==RF_NOT_FOUND);
        CHECK(!memcmp(&bad,&saved,sizeof(bad)));
    }
    weapon=weapon_before;live.occupied=1;staged=sentinel;
    CHECK(scene_fighter_checkpoint_stage(&live,&record,&backend,&staged)==RF_NOT_FOUND);
    CHECK(!memcmp(&staged,&sentinel,sizeof(staged)));live.occupied=0;
    bad=record;bad.class_id=1;CHECK(scene_fighter_checkpoint_validate(&bad)==RF_FORMAT);
    bad=record;bad.primary_reserve=901;CHECK(scene_fighter_checkpoint_validate(&bad)==RF_FORMAT);
    bad=record;bad.rocket_fire.cooldown=3.1f;CHECK(scene_fighter_checkpoint_validate(&bad)==RF_FORMAT);
    bad=record;bad.vehicle.health=-1;bad.vehicle.alive=0;
    CHECK(scene_fighter_checkpoint_stage(&live,&bad,&backend,&staged)==RF_NOT_FOUND);
    puts("Fighter staging: airborne pose/momentum, finite ammo/cooldowns, occupancy and transactional admission passed");return 0;
}
