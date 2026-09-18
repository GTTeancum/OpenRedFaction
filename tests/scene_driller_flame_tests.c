/* Installed chassis/material factors + real flame volume, pose, world cover
 * and host damage. No visual/burn-persistence claim and no new damage policy. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_driller_flame.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Driller flame line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    const float basis[9]={1,0,0,0,1,0,0,0,1},position[3]={0,10,0},eye[3]={0},forward[3]={0,0,1};
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *resource=NULL;
    scene_driller_physics physics;scene_driller_damage prototype;scene_driller_damage_runtime owner={0};
    scene_driller_entry_exit entry={0};scene_driller_player_adapter player={0};scene_vehicle_collision collision={0};
    rf_physics_sphere player_sphere={{0},.4f,0,0};scene_stream stream={0};rf_geometry_collision_world world={0};
    rf_collision_solid_view mover={0};rf_collision_face face={0};
    float vertices[4][3]={{-10,0,-8},{-10,20,-8},{10,20,-8},{10,0,-8}};
    rf_weapon_primary_definition definition;rf_flame_cadence cadence={0};rf_damage_request request,excluded;
    float origin[3]={0,10.674f,-9},target[3]={0,10.674f,3},amount,before;
    uint32_t i,pulse,hit,blocked,prior;char path[128];
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resource));
    CHECK(!scene_driller_physics_initialize(resource,position,basis,9.8f,&physics));
    CHECK(!scene_driller_damage_open(&prototype,&tables,resource,1,0,2,2*1024*1024));
    CHECK(!rf_weapon_primary_load(&tables,"Flamethrower",2*1024*1024,&definition));CHECK(definition.damage_kind==4);
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    campaign_player_view.flags_7c=8;campaign_player_view.linked_handle=-1;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_damage.state.effects.handle=campaign_player_object.handle;
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.class_health=100;
    campaign_player_damage.state.effects.affiliation=1;
    scene_actor_body.spheres.items=&player_sphere;scene_actor_body.spheres.count=1;
    memcpy(scene_actor_body.state.orientation,basis,36);memcpy(scene_actor_body.state.local_tensor,basis,36);
    collision.spheres=physics.collision;collision.count=physics.sphere_count;collision.radius=physics.radius;
    CHECK(!scene_driller_entry_exit_open(&entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,resource,&physics,&collision,eye));
    CHECK(!scene_driller_damage_runtime_open(&owner,&entry,&player,&prototype));stream.collision=&world;
    CHECK(!rf_flame_pulse(&cadence,&definition,0,1,campaign_player_object.handle,&pulse,&request)&&pulse);
    before=owner.damage.state.effects.health;prior=owner.damage_calls;
    CHECK(!scene_driller_flame_pulse(&stream,0,origin,forward,&request,&hit,&amount)&&hit);
    CHECK(owner.damage_calls==prior+1); /* Two rear spheres overlap volume: one damage dispatch. */
    CHECK(fabsf(amount-definition.damage*prototype.factors[4])<.001f);
    CHECK(fabsf(owner.damage.state.effects.health-(before-amount))<.001f);
    CHECK(!rf_flame_pulse(&cadence,&definition,0,1,campaign_player_object.handle,&pulse,&excluded)&&!pulse);
    CHECK(!scene_driller_flame_cover(&stream,request.source,origin,target,&blocked)&&blocked);
    face.vertices=vertices;face.count=4;face.plane[2]=-1;face.plane[3]=-8;
    for(i=0;i<3;i++){face.minimum[i]=vertices[0][i];face.maximum[i]=vertices[2][i];mover.input_matrix[i][i]=mover.output_matrix[i][i]=1;}
    mover.minimum[0]=-10;mover.maximum[0]=10;mover.minimum[1]=0;mover.maximum[1]=20;
    mover.minimum[2]=-9;mover.maximum[2]=-7;mover.flat_faces=&face;mover.flat_count=1;
    campaign_movers.views=&mover;campaign_movers.count=1;prior=owner.damage_calls;
    CHECK(!scene_driller_flame_pulse(&stream,1,origin,forward,&request,&hit,&amount)&&!hit&&amount==0);
    CHECK(owner.damage_calls==prior);
    campaign_movers.count=0;
    excluded=request;excluded.source=entry.host.handle;
    CHECK(!scene_driller_flame_pulse(&stream,2,origin,forward,&excluded,&hit,&amount)&&!hit);
    /* Set an occupied ownership snapshot; entry/exit mechanics have their own test. */
    entry.session.active=1;entry.host.driver=campaign_player_object.handle;
    campaign_player_view.linked_handle=(int32_t)entry.host.handle;
    CHECK(!scene_driller_flame_pulse(&stream,3,origin,forward,&request,&hit,&amount)&&!hit);
    CHECK(!scene_driller_flame_cover(&stream,request.source,origin,target,&blocked)&&!blocked);
    entry.session.active=0;entry.host.driver=UINT32_MAX;campaign_player_view.linked_handle=-1;
    physics.state.position[0]+=40;
    CHECK(!scene_driller_flame_pulse(&stream,4,origin,forward,&request,&hit,&amount)&&!hit);
    CHECK(!scene_driller_flame_cover(&stream,request.source,origin,target,&blocked)&&!blocked);
    CHECK(owner.damage_calls==prior);
    printf("Driller flame: authored fire factor%g, one union dose, real wall cover, host/occupant exclusions, chassis occlusion\n",prototype.factors[4]);
    scene_driller_damage_runtime_close(&owner);CHECK(!scene_driller_entry_exit_close(&entry,1));
    scene_driller_resources_close(&resource);rf_vpp_close(&tables);rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    return 0;
}
