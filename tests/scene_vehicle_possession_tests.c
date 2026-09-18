#include "rf/entity.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include "../src/diagnostic/scene_vehicle_possession.inc"
typedef struct fixture {uint32_t allowed,clear,seat_calls,exit_calls;int status;scene_vehicle_pose seat,exit;} fixture;
static int allowed(void *p,uint32_t player,uint32_t host,uint32_t *out){assert(player==17 && host==99);*out=((fixture*)p)->allowed;return RF_OK;}
static int seat(void *p,uint32_t host,scene_vehicle_pose *out){fixture *f=p;assert(host==99);++f->seat_calls;*out=f->seat;return f->status;}
static int exit_pose(void *p,uint32_t player,uint32_t host,scene_vehicle_pose *out,uint32_t *clear){fixture *f=p;assert(player==17 && host==99);++f->exit_calls;*out=f->exit;*clear=f->clear;return f->status;}
int main(void)
{
    fixture f={0};scene_vehicle_possession session={0};uint32_t changed;
    scene_vehicle_player_control player={0},saved,seated;
    scene_vehicle_host_control host={99,UINT32_MAX,1};
    scene_vehicle_possession_ops ops={allowed,seat,exit_pose,&f};
    player.handle=player.control=17;player.host=UINT32_MAX;player.movement=1;player.weapon=6;player.control_flags=23;
    player.pose.position[0]=3;player.pose.basis[0]=player.pose.basis[4]=player.pose.basis[8]=1;saved=player;
    f.seat=player.pose;f.seat.position[0]=10;f.exit=player.pose;f.exit.position[0]=12;
    assert(!scene_vehicle_possess(&session,&player,&host,1,&ops,&changed) && !changed && !f.seat_calls);
    f.allowed=1;f.status=RF_NOT_FOUND;
    assert(scene_vehicle_possess(&session,&player,&host,1,&ops,&changed)==RF_NOT_FOUND && !memcmp(&player,&saved,sizeof(player)) && host.driver==UINT32_MAX);
    f.status=0;assert(!scene_vehicle_possess(&session,&player,&host,1,&ops,&changed) && changed);
    assert(session.active && player.host==99 && player.control==99 && host.driver==17 && player.pose.position[0]==10);
    seated=player;
    assert(!scene_vehicle_possess(&session,&player,&host,1,&ops,&changed) && !changed);
    assert(!scene_vehicle_unpossess(&session,&player,&host,0,&ops,&changed) && !changed && !memcmp(&player,&seated,sizeof(player)));
    assert(!scene_vehicle_unpossess(&session,&player,&host,1,&ops,&changed) && !changed && session.active);
    host.handle=100;assert(scene_vehicle_unpossess(&session,&player,&host,2,&ops,&changed)==RF_NOT_FOUND);host.handle=99;
    f.clear=1;assert(!scene_vehicle_unpossess(&session,&player,&host,0,&ops,&changed) && changed);
    saved.pose=f.exit;assert(!memcmp(&player,&saved,sizeof(player)) && host.driver==UINT32_MAX && !session.active);
    assert(!scene_vehicle_possess(&session,&player,&host,1,&ops,&changed) && changed);
    host.alive=0;f.clear=0;
    assert(!scene_vehicle_unpossess(&session,&player,&host,1,&ops,&changed) && !changed && session.active);
    {uint32_t calls=f.exit_calls;assert(!scene_vehicle_unpossess(&session,&player,&host,2,NULL,&changed) && changed);assert(calls==f.exit_calls);}
    assert(!session.active && host.driver==UINT32_MAX && player.host==UINT32_MAX && player.control==17 && player.pose.position[0]==10);
    assert(!scene_vehicle_possess(&session,&player,&host,1,&ops,&changed) && !changed);
    return 0;
}
