/* Typed capture with actual scene globals/registry and bounded metadata fixture;
 * resource loading and physical placement are tested separately. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_driller_checkpoint_adapter.inc"
#include "../src/diagnostic/scene_vehicle_combat_checkpoint.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vehicle combat capture line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static scene_stream stream;static scene_driller_runtime runtime;static scene_driller_resources resource;
    rf_model_attachment tags[2]={{0}};rf_apc_checkpoint apc,apc_saved;rf_jeep_checkpoint jeep,jeep_saved;
    uint32_t i;float basis[9]={0,0,-1,0,1,0,1,0,0};
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&runtime.entry.view,&runtime.entry.registration));
    runtime.entry.entities=&campaign_entities;runtime.entry.physics=&runtime.physics;runtime.entry.resource=&resource;
    runtime.entry.player_view=&campaign_player_view;runtime.entry.host.handle=runtime.entry.registration.handle;
    runtime.entry.host.alive=1;runtime.damage.active=1;runtime.damage.entry=&runtime.entry;
    runtime.damage.damage.state.effects.handle=runtime.entry.registration.handle;
    for(i=0;i<3;i++){runtime.physics.state.orientation[i*3+i]=1;runtime.physics.state.inverse_inertia[i*3+i]=.5f;}
    runtime.physics.state.momentum[1]=2;stream.driller_runtime=&runtime;stream.driller=&resource;
    strcpy(resource.model,"APC.v3m");runtime.damage.damage.state.effects.health=5000;
    stream.apc_primary.reserve=987;stream.apc_secondary.reserve=13;stream.apc_primary.random.value=12345;
    stream.apc_aim.angles[0]=.4f; /* Unoccupied capture canonicalizes stale aim. */
    CHECK(!scene_apc_checkpoint_capture(&stream,&apc));CHECK(apc.vehicle.health==5000 && apc.vehicle.angular_velocity[1]==1);
    CHECK(apc.primary_reserve==987 && apc.secondary_reserve==13 && apc.primary_random==12345 && apc.aim_pitch==0);
    apc_saved=apc;stream.apc_primary.rounds[0].flight.active=1;
    CHECK(scene_apc_checkpoint_capture(&stream,&apc)==RF_NOT_FOUND && !memcmp(&apc,&apc_saved,sizeof(apc)));
    stream.apc_primary.rounds[0].flight.active=0;stream.apc_primary.scheduler.warmup=.05f;
    CHECK(scene_apc_checkpoint_capture(&stream,&apc)==RF_NOT_FOUND);stream.apc_primary.scheduler.warmup=0;
    stream.apc_secondary.cooldown=.1f;CHECK(scene_apc_checkpoint_capture(&stream,&apc)==RF_NOT_FOUND);stream.apc_secondary.cooldown=0;
    runtime.damage.damage.state.effects.burn=1;CHECK(scene_apc_checkpoint_capture(&stream,&apc)==RF_NOT_FOUND);runtime.damage.damage.state.effects.burn=0;
    strcpy(resource.model,"Jeep01.v3m");runtime.damage.damage.state.effects.health=400;
    strcpy(tags[0].name,"interface_1");strcpy(tags[1].name,"interface_2");resource.tags.items=tags;resource.tags.count=2;resource.seat=1;
    runtime.jeep_seat.role=1;runtime.entry.session.active=1;
    runtime.entry.player.handle=campaign_player_object.handle;runtime.entry.session.driver=runtime.entry.player.handle;
    runtime.entry.session.host=runtime.entry.host.handle;runtime.entry.host.driver=runtime.entry.player.handle;
    runtime.entry.player.host=runtime.entry.host.handle;runtime.entry.occupant=(int32_t)runtime.entry.player.handle;
    campaign_player_view.linked_handle=(int32_t)runtime.entry.host.handle;
    memcpy(scene_actor_body.state.orientation,basis,36);stream.apc_aim_active=1;stream.apc_aim.angles[1]=.7f;
    CHECK(!scene_jeep_checkpoint_capture(&stream,&jeep));CHECK(jeep.vehicle.health==400 && jeep.role==1 && jeep.aim_pitch==.4f && jeep.aim_yaw==.7f);
    CHECK(!memcmp(jeep.aim_reference,basis,36) && jeep.primary_reserve==987 && jeep.primary_random==12345);
    jeep_saved=jeep;runtime.jeep_seat.held=1;CHECK(scene_jeep_checkpoint_capture(&stream,&jeep)==RF_NOT_FOUND);
    CHECK(!memcmp(&jeep,&jeep_saved,sizeof(jeep)));runtime.jeep_seat.held=0;
    runtime.damage.damage.state.effects.health=401;CHECK(scene_jeep_checkpoint_capture(&stream,&jeep)==RF_FORMAT);
    CHECK(!memcmp(&jeep,&jeep_saved,sizeof(jeep)));runtime.damage.damage.state.effects.health=400;
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&runtime.entry.registration));
    CHECK(scene_jeep_checkpoint_capture(&stream,&jeep)==RF_NOT_FOUND && !memcmp(&jeep,&jeep_saved,sizeof(jeep)));
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object));
    puts("PASS typed APC/Jeep capture, health bounds, independent gunner basis, transients, stale handle, atomic output");return 0;
}
