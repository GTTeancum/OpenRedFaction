#ifndef RF_SCENE_SECONDARY_VEHICLE_ORDER_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_ORDER_INCLUDED
#include "rf/event.h"
#include "rf/vehicle_submersible.h"
#include <math.h>
#include <string.h>
enum {SCENE_SECONDARY_ORDER_NONE,SCENE_SECONDARY_ORDER_POINT,SCENE_SECONDARY_ORDER_PLAYER,SCENE_SECONDARY_ORDER_ATTACK};
/* Profile6 is masako_fighter/Fighter02, never the player Fighter01 profile5.
 * Both authored classes use movement9; only their dry-flight solver is shared. */
static inline uint32_t scene_secondary_vehicle_flight_profile(uint32_t profile)
{return profile==4||profile==5||profile==6;}
static inline uint32_t scene_secondary_vehicle_aircraft_profile(uint32_t profile)
{return profile==5||profile==6;}
/* Parsed type, never editor display name, selects the order. Require both
 * original UID and resolved owner identity. APC retains fixed Goto only;
 * submarines and regular Fighters admit live-player pursuit through their
 * distinct wet/dry hull solvers. Masako's original17973 is type5 Goto despite
 * its editor name Goto_Player; admit the fixed point only, without inventing
 * a boss pursuit or Attack order. */
static int scene_secondary_vehicle_order_event(const rf_runtime_event *event,
    uint32_t profile,uint32_t uid,uint32_t handle,uint32_t *kind)
{
    const rf_level_owned_event *authored;uint32_t linked=0,original=0,order;int32_t type;
    if(!event||!event->authored||!kind)return RF_RANGE;
    authored=event->authored;type=rf_event_type_id(authored->record.type);
    if((profile!=2&&!scene_secondary_vehicle_flight_profile(profile))||event->state.type!=(uint32_t)type)return RF_FORMAT;
    if(type==38&&profile==5){
        if(authored->record.words[0]!=uid)return RF_NOT_FOUND;
        *kind=SCENE_SECONDARY_ORDER_ATTACK;return RF_OK;
    }
    if(type==5)order=SCENE_SECONDARY_ORDER_POINT;
    else if(type==6&&(profile==4||profile==5))order=SCENE_SECONDARY_ORDER_PLAYER;
    else return RF_NOT_FOUND;
    if(!authored->record.link_count||!authored->links||!event->links)return RF_FORMAT;
    for(uint32_t i=0;i<authored->record.link_count;i++){
        if(authored->links[i]==uid)++original;
        if((event->links[i].kind==1||event->links[i].kind==2)&&event->links[i].value==handle)++linked;
    }
    if(original!=1||linked!=1)return RF_FORMAT;
    *kind=order;return RF_OK;
}
/* First-pass submarine/Fighter point steering. Submarine gains match the
 * existing selected-host consumer; Fighter gains remain unchanged. A missing live player yields neutral controls without completing the
 * order. Target sampling never modifies the authored point or any rigid state.
 * Arrival completes only a fixed Goto; pursuit stays armed inside tolerance. */
static int scene_secondary_vehicle_point_steer(uint32_t profile,const rf_vehicle_rigid_state *state,
    uint32_t kind,const float authored[3],const float player[3],
    rf_vehicle_submersible_command *command,uint32_t *complete)
{
    rf_vehicle_submersible_command out={0};uint32_t done=0;
    const float *target;float dx,dy,dz,distance,horizontal,angle;
    if(!state||!authored||!command||!complete||!scene_secondary_vehicle_flight_profile(profile))return RF_RANGE;
    if(kind!=SCENE_SECONDARY_ORDER_POINT&&kind!=SCENE_SECONDARY_ORDER_PLAYER)return RF_FORMAT;
    if(profile==6&&kind!=SCENE_SECONDARY_ORDER_POINT)return RF_FORMAT;
    out.controlled=1;target=kind==SCENE_SECONDARY_ORDER_PLAYER?player:authored;
    if(!target){*command=out;*complete=0;return RF_OK;}
    dx=target[0]-state->position[0];dy=target[1]-state->position[1];dz=target[2]-state->position[2];
    distance=sqrtf(dx*dx+dy*dy+dz*dz);horizontal=sqrtf(dx*dx+dz*dz);
    if(!isfinite(distance)||!isfinite(horizontal))return RF_FORMAT;
    if(profile==4?(horizontal<=1.5f&&fabsf(dy)<=1.5f):distance<=3.f)done=kind==SCENE_SECONDARY_ORDER_POINT;
    else{
        angle=atan2f(state->orientation[8]*dx-state->orientation[6]*dz,
            state->orientation[6]*dx+state->orientation[8]*dz);
        if(!isfinite(angle))return RF_FORMAT;
        out.yaw=fmaxf(-1.f,fminf(1.f,angle/.7f));
        out.throttle=fabsf(angle)>1.5f?0.f:fabsf(angle)>.8f?.2f:fminf(1.f,horizontal/(profile==4?4.f:8.f));
        out.rise=fmaxf(-1.f,fminf(1.f,dy/(profile==4?3.f:6.f)));
    }
    *command=out;*complete=done;return RF_OK;
}
static int scene_secondary_vehicle_fighter_steer(const rf_vehicle_rigid_state *state,
    uint32_t kind,const float authored[3],const float player[3],
    rf_vehicle_submersible_command *command,uint32_t *complete)
{return scene_secondary_vehicle_point_steer(5,state,kind,authored,player,command,complete);}
#endif
