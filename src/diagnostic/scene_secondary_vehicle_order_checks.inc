#ifndef RF_SCENE_SECONDARY_VEHICLE_ORDER_CHECKS_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_ORDER_CHECKS_INCLUDED
#include "scene_secondary_vehicle_order.inc"
/* Shared pure checks for host compile and an opt-in Xbox observer. No live
 * scene mutation, event execution, registry changes or player placement.
 * out: attempted,passed,failure mask,first failing source line; reserved12. */
static int scene_secondary_vehicle_order_checks(uint32_t out[16])
{
    rf_level_owned_event authored={0};rf_runtime_event event={0};
    uint32_t uids[2]={18155,18155},kind=99,done=99;rf_level_link_target links[2]={{123,1,0},{123,1,0}};
    rf_vehicle_rigid_state state={0},before;rf_vehicle_submersible_command command={0},saved;
    float point[3]={0,0,10},player[3]={10,6,10},original[3];int status;
    if(!out)return RF_RANGE;memset(out,0,16*sizeof(*out));
#define SV_ORDER_CHECK(bit,condition) do{++out[0];if(condition)++out[1];else{out[2]|=1u<<(bit);if(!out[3])out[3]=__LINE__;}}while(0)
    authored.record.uid=18156;authored.record.link_count=1;authored.links=uids;
    event.authored=&authored;event.links=links;event.state.type=5;
    strcpy(authored.record.type,"Goto");strcpy(authored.record.name,"Goto_Player");
    status=scene_secondary_vehicle_order_event(&event,5,18155,123,&kind);
    SV_ORDER_CHECK(0,!status&&kind==SCENE_SECONDARY_ORDER_POINT);
    strcpy(authored.record.type,"Goto_Player");strcpy(authored.record.name,"Goto");event.state.type=6;
    status=scene_secondary_vehicle_order_event(&event,5,18155,123,&kind);
    SV_ORDER_CHECK(1,!status&&kind==SCENE_SECONDARY_ORDER_PLAYER);
    kind=99;status=scene_secondary_vehicle_order_event(&event,2,18155,123,&kind);
    SV_ORDER_CHECK(2,status==RF_NOT_FOUND&&kind==99);
    event.state.type=5;status=scene_secondary_vehicle_order_event(&event,5,18155,123,&kind);
    SV_ORDER_CHECK(3,status==RF_FORMAT&&kind==99);event.state.type=6;
    uids[0]=18167;status=scene_secondary_vehicle_order_event(&event,5,18155,123,&kind);
    SV_ORDER_CHECK(4,status==RF_FORMAT&&kind==99);uids[0]=18155;
    links[0].value=124;status=scene_secondary_vehicle_order_event(&event,5,18155,123,&kind);
    SV_ORDER_CHECK(5,status==RF_FORMAT&&kind==99);links[0].value=123;
    authored.record.link_count=2;status=scene_secondary_vehicle_order_event(&event,5,18155,123,&kind);
    SV_ORDER_CHECK(6,status==RF_FORMAT&&kind==99);authored.record.link_count=1;
    state.orientation[0]=state.orientation[4]=state.orientation[8]=1;before=state;memcpy(original,point,12);
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_POINT,point,player,&command,&done);
    SV_ORDER_CHECK(7,!status&&!done&&command.controlled==1&&command.throttle==1&&command.yaw==0&&command.rise==0);
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_PLAYER,point,player,&command,&done);
    uint32_t right=!status&&!done&&command.yaw>0&&command.rise==1&&command.throttle>0;
    player[0]=-10;player[1]=-6;
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_PLAYER,point,player,&command,&done);
    SV_ORDER_CHECK(8,right&&!status&&!done&&command.yaw<0&&command.rise==-1&&command.throttle>0&&
        !memcmp(point,original,12)&&!memcmp(&state,&before,sizeof(state)));
    player[0]=player[1]=0;player[2]=3;
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_PLAYER,point,player,&command,&done);
    uint32_t waiting=!status&&!done&&command.controlled&&!command.yaw&&!command.rise&&!command.throttle;
    player[2]=10;
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_PLAYER,point,player,&command,&done);
    SV_ORDER_CHECK(9,waiting&&!status&&!done&&command.throttle==1&&!command.yaw&&!command.rise);
    point[2]=3;
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_POINT,point,player,&command,&done);
    SV_ORDER_CHECK(10,!status&&done==1&&command.controlled&&!command.yaw&&!command.rise&&!command.throttle);
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_PLAYER,point,NULL,&command,&done);
    SV_ORDER_CHECK(11,!status&&!done&&command.controlled&&!command.yaw&&!command.rise&&!command.throttle);
    saved=command;done=99;player[0]=NAN;
    status=scene_secondary_vehicle_fighter_steer(&state,SCENE_SECONDARY_ORDER_PLAYER,point,player,&command,&done);
    SV_ORDER_CHECK(12,status==RF_FORMAT&&done==99&&!memcmp(&command,&saved,sizeof(saved))&&
        !memcmp(&state,&before,sizeof(state)));
#undef SV_ORDER_CHECK
    return out[2]?RF_FORMAT:RF_OK;
}
#endif
