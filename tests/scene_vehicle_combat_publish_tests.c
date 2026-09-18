/* Real registered scene publication/admission, minimal borrowed resource
 * metadata. No terrain commit or installed rendering claimed. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"combat publish line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    static scene_stream stream;static scene_driller_runtime runtime,unchanged;
    static scene_driller_resources resources;rf_model_attachment tag={0};scene_driller_damage prototype={0};
    rf_apc_checkpoint record={0};scene_vehicle_combat_candidate candidate,bad;scene_apc_primary_state primary_before;
    rf_physics_sphere player_sphere={{0,0,0},.4f,0,0};rf_collision_body_sphere host_sphere={{0,0,0},1};scene_vehicle_collision collision={0};
    const float identity[9]={1,0,0,0,1,0,0,0,1},eye[3]={0};uint32_t handle;int32_t original_seat;
    rf_scene_vehicle_enabled=2;strcpy(resources.model,"APC.v3m");strcpy(tag.name,"interface_1");tag.rotation[3]=1;
    resources.tags.items=&tag;resources.tags.count=1;resources.seat=0;
    memcpy(runtime.physics.state.orientation,identity,36);memcpy(runtime.physics.state.inverse_inertia,identity,36);
    memcpy(record.vehicle.orientation,identity,36);record.vehicle.health=5000;record.vehicle.alive=1;record.vehicle.position[0]=20;
    record.primary_reserve=123;record.secondary_reserve=7;record.primary_random=987;record.aim_pitch=.2f;
    CHECK(!scene_apc_checkpoint_stage_rigid(&runtime.physics.state,&record,&candidate));
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_view.linked_handle=-1;campaign_player_damage.state.effects.health=100;
    scene_actor_body.spheres.items=&player_sphere;scene_actor_body.spheres.count=1;
    collision.spheres=&host_sphere;collision.count=1;collision.radius=1;
    CHECK(!scene_driller_entry_exit_open(&runtime.entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,&resources,&runtime.physics,&collision,eye));
    prototype.state.effects.health=prototype.state.effects.class_health=5000;prototype.state.effects.affiliation=2;
    CHECK(!scene_driller_damage_runtime_open(&runtime.damage,&runtime.entry,&runtime.player,&prototype));
    stream.driller=&resources;stream.driller_runtime=&runtime;stream.apc_primary.reserve=999;stream.apc_secondary.reserve=15;
    stream.apc_primary.definition.damage=150;stream.apc_primary.muzzle=17;stream.apc_aim_limits.maximum[0]=.8f;
    unchanged=runtime;primary_before=stream.apc_primary;original_seat=resources.seat;
    CHECK(!scene_vehicle_combat_parked_admit(&stream,&candidate));
    CHECK(!memcmp(&runtime,&unchanged,sizeof(runtime))&&!memcmp(&stream.apc_primary,&primary_before,sizeof(primary_before))&&resources.seat==original_seat);
    /* The concrete precommit hazard: an outstanding mortar now fails BEFORE
     * any common vehicle/ammo/aim publication, exactly like actual apply. */
    stream.apc_secondary.rounds[0].flight.active=1;
    CHECK(scene_vehicle_combat_parked_admit(&stream,&candidate)==RF_NOT_FOUND);
    CHECK(scene_vehicle_combat_apply_parked(&stream,&candidate)==RF_NOT_FOUND);
    CHECK(!memcmp(&runtime,&unchanged,sizeof(runtime))&&stream.apc_primary.reserve==999&&stream.apc_secondary.reserve==15);
    stream.apc_secondary.rounds[0].flight.active=0;stream.apc_primary.scheduler.cooldown=.2f;
    CHECK(scene_vehicle_combat_parked_admit(&stream,&candidate)==RF_NOT_FOUND);
    stream.apc_primary.scheduler.cooldown=0;
    bad=candidate;bad.aim_reference[0]=NAN;
    CHECK(scene_vehicle_combat_parked_admit(&stream,&bad)==RF_FORMAT);
    CHECK(scene_vehicle_combat_apply_parked(&stream,&bad)==RF_FORMAT);
    CHECK(!memcmp(&runtime,&unchanged,sizeof(runtime))&&resources.seat==original_seat);
    handle=runtime.entry.registration.handle;
    CHECK(!scene_vehicle_combat_apply_parked(&stream,&candidate));
    CHECK(runtime.physics.state.position[0]==20&&runtime.damage.damage.state.effects.health==5000);
    CHECK(runtime.entry.registration.handle==handle&&runtime.damage.damage.state.effects.handle==handle);
    CHECK(stream.apc_primary.reserve==123&&stream.apc_secondary.reserve==7&&stream.apc_primary.random.value==987);
    CHECK(stream.apc_primary.definition.damage==150&&stream.apc_primary.muzzle==17&&stream.apc_aim_limits.maximum[0]==.8f);
    CHECK(stream.apc_aim.angles[0]==.2f&&!stream.apc_aim_active&&!runtime.entry.session.active);
    scene_driller_damage_runtime_close(&runtime.damage);CHECK(!scene_driller_entry_exit_close(&runtime.entry,1));
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object));
    scene_actor_body.spheres.items=NULL;scene_actor_body.spheres.count=0;
    puts("PASS shared parked precommit admission, pending flight/cooldown/aim rejection, atomic ammo/host publication");return 0;
}
