#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_event_gameplay.inc"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t teleport_dispatch_calls;
static int teleport_dispatch_result;
static int teleport_dispatch(void *context,const rf_level_event *event)
{
    if(context!=&teleport_dispatch_calls || event->uid!=63)return RF_FORMAT;
    ++teleport_dispatch_calls;return teleport_dispatch_result;
}
static int teleport_event_dispatch(void)
{
    rf_object_registry registry;rf_runtime_triggers triggers={0};rf_runtime_events events={0};
    rf_runtime_event event={0};rf_level_owned_event authored={0};rf_startup_events_report report;
    rf_physics_gravity gravity={0};uint32_t pending;
    rf_object_registry_init(&registry);triggers.registry=&registry;events.registry=&registry;
    events.items=&event;events.count=1;event.object_kind=6;event.authored=&authored;
    event.state.type=63;event.state.deadline=-1;authored.record.uid=63;
    CHECK(rf_object_registry_insert(&registry,&event,&event.handle)==RF_OK);
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,8,100,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(report.unsupported_actions==1 && !teleport_dispatch_calls);
    triggers.teleport_player=teleport_dispatch;triggers.teleport_context=&teleport_dispatch_calls;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,8,100,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(!report.unsupported_actions && teleport_dispatch_calls==1);
    event.state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,8,100,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(teleport_dispatch_calls==1 && event.state.deadline==350);
    triggers.teleport_player=NULL;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,350,NULL,NULL,&report,&pending)==RF_OK && pending==1);
    triggers.teleport_player=teleport_dispatch;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,350,NULL,NULL,&report,&pending)==RF_OK);
    CHECK(!pending && teleport_dispatch_calls==2 && event.state.deadline==-1);
    event.state.delay=0;event.state.flags|=1;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,8,400,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(teleport_dispatch_calls==2);event.state.flags&=~1u;
    teleport_dispatch_result=RF_NOT_FOUND;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,8,500,&gravity,NULL,NULL,&report)==RF_OK);
    CHECK(report.other_targets==1 && teleport_dispatch_calls==3);
    teleport_dispatch_result=RF_IO;
    CHECK(rf_runtime_event_fire(&triggers,event.handle,7,8,600,&gravity,NULL,NULL,&report)==RF_IO);
    return 0;
}
int main(void)
{
    CHECK(teleport_event_dispatch()==0);
    rf_level_event event={0};rf_physics_body_state body={0},saved;
    rf_group_attached_pose pose={0},saved_pose;rf_look_pose look={0},saved_look;
    uint32_t i;float identity[9]={1,0,0,0,1,0,0,0,1};
    event.has_orientation=1;event.position[0]=10;event.position[1]=20;event.position[2]=-3;
    /* Disk forward/right/up; destination yaw90. */
    event.orientation_disk[0]=1;event.orientation_disk[5]=-1;event.orientation_disk[7]=1;
    body.bounds.radius=.75f;body.velocity[0]=2;body.velocity[1]=-3;body.vector_c8[2]=4;
    memcpy(body.local_tensor,identity,36);look.state.pending_pitch=.02f;
    CHECK(campaign_teleport_prepare(&event,&body,&pose,&look)==RF_OK);
    CHECK(!memcmp(body.position,event.position,12) && !memcmp(body.next_position,event.position,12));
    CHECK(!memcmp(pose.public_position,event.position,12) && !memcmp(pose.pending,event.position,12));
    CHECK(body.velocity[0]==2 && body.velocity[1]==-3 && body.vector_c8[2]==4);
    CHECK(body.bounds.minimum[1]==19.25f && body.bounds.maximum[1]==20.75f);
    CHECK(body.orientation[6]==1 && body.orientation[2]==-1);
    CHECK(!memcmp(body.orientation,look.eye_orientation,36) && !memcmp(body.orientation,pose.output_matrix,36));
    CHECK(fabsf(look.state.body_angles[1]-1.57079637f)<.00001f && look.state.eye_angles[1]==0);
    CHECK(look.state.pending_pitch==.02f);
    saved=body;saved_pose=pose;saved_look=look;event.position[0]=NAN;
    CHECK(campaign_teleport_prepare(&event,&body,&pose,&look)==RF_FORMAT);
    CHECK(!memcmp(&saved,&body,sizeof(body)) && !memcmp(&saved_pose,&pose,sizeof(pose)) && !memcmp(&saved_look,&look,sizeof(look)));
    event.position[0]=10;scene_actor_body.state=saved;scene_actor_body.allocated_bytes=1;
    rf_scene_actor_pose=saved_pose;actor_look=saved_look;campaign_player_view.linked_handle=-1;
    rf_scene_actor_landing[1]=1;campaign_support_handle=42;campaign_support_velocity[1]=5;campaign_piece_support.tag=1;
    campaign_teleport_ready=0;event.position[0]=30;
    CHECK(campaign_teleport_player(NULL,&event)==RF_OK && campaign_teleport_pending);
    CHECK(scene_actor_body.state.position[0]==10);
    CHECK(campaign_teleport_begin()==RF_OK && !campaign_teleport_pending);
    CHECK(scene_actor_body.state.position[0]==30 && rf_scene_actor_pose.public_position[0]==30);
    CHECK(!campaign_support_handle && !campaign_piece_support.tag && !campaign_support_velocity[1]);
    CHECK(rf_scene_actor_landing[1]==3 && scene_actor_body.state.velocity[0]==2);
    event.position[0]=40;CHECK(campaign_teleport_player(NULL,&event)==RF_OK);
    CHECK(scene_actor_body.state.position[0]==30);
    CHECK(campaign_teleport_begin()==RF_OK && scene_actor_body.state.position[0]==40);
    /* A normal next look tick retains destination yaw rather than doubling it. */
    actor_look.state.pending_pitch=0;CHECK(rf_look_update_pose(&actor_look.state,1,1.0f/60,&actor_look)==RF_OK);
    CHECK(fabsf(actor_look.eye_orientation[6]-1)<.00001f && fabsf(actor_look.eye_orientation[8])<.00001f);
    campaign_player_view.linked_handle=77;event.position[0]=50;
    CHECK(campaign_teleport_player(NULL,&event)==RF_NOT_FOUND && scene_actor_body.state.position[0]==40);
    for(i=0;i<3;i++)CHECK(scene_actor_body.state.velocity[i]==saved.velocity[i]);
    puts("Teleport pose, startup queue, camera continuation, support invalidation and velocity checks passed");return 0;
}
