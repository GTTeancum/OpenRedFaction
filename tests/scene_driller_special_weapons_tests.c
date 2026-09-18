/* Installed registered Driller + actual scene precision/shotgun routing,
 * transformed chassis spheres, real finite mover cover and host damage.
 * Only the optional downstream NPC precision publication is recorded: this
 * proves rail continuation without claiming NPC animation/pain setup here. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Driller special weapons line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t npc_publications,npc_published_handle;
static int record_npc_damage(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock,const rf_damage_effect_backend *effects,float *amount)
{
    (void)difficulty;(void)clock;(void)effects;
    if(request->source!=campaign_player_object.handle||request->amount!=campaign_pistol.damage)return RF_FORMAT;
    ++npc_publications;npc_published_handle=handle;*amount=0;return RF_OK;
}
#define scene_precision_contact recorded_precision_contact
#define scene_precision_insert recorded_precision_insert
#define scene_precision_fire recorded_precision_fire
#define rf_scene_npc_damage record_npc_damage
#include "../src/diagnostic/scene_precision_gameplay.inc"
#undef rf_scene_npc_damage
#undef scene_precision_fire
#undef scene_precision_insert
#undef scene_precision_contact
static void quiet_notify(void *context,uint32_t kind,uint32_t target,float value,uint32_t source)
{(void)context;(void)kind;(void)target;(void)value;(void)source;}
int main(void)
{
    const float identity[9]={1,0,0,0,1,0,0,0,1},origin[3]={0,10,0},eye[3]={0,0,0},forward[3]={0,0,1};
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *resources=NULL;
    scene_driller_physics physics;scene_driller_damage prototype;scene_driller_damage_runtime host={0};
    scene_driller_entry_exit entry={0};scene_driller_player_adapter player={0};scene_vehicle_collision collision={0};
    scene_stream scene={0};rf_geometry_collision_world world={0};
    rf_collision_solid_view mover={0};rf_collision_face face={0};
    float vertices[4][3]={{-5,8,-10},{-5,14,-10},{5,14,-10},{5,8,-10}};
    rf_physics_sphere player_sphere={{0,0,0},.4f,0,0},npc_sphere={{0,0,0},.5f,0,0};
    campaign_npc_body shooter={0},npc={0};rf_weapon_primary_definition sniper,rail,shotgun;
    combat_feedback feedback={0};float start[3],delta[3],before,amount,player_before;uint32_t i,prior;char path[128];
    rf_damage_effect_backend effects={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
        campaign_damage_test_burn,campaign_damage_test_random,quiet_notify,campaign_damage_test_playing,campaign_damage_test_play,NULL};
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resources));
    CHECK(!scene_driller_physics_initialize(resources,origin,identity,9.8f,&physics));
    CHECK(!scene_driller_damage_open(&prototype,&tables,resources,1,0,2,2*1024*1024));
    CHECK(!rf_weapon_primary_load(&tables,"Sniper Rifle",2*1024*1024,&sniper));
    CHECK(!rf_weapon_primary_load(&tables,"rail_gun",2*1024*1024,&rail));
    CHECK(!rf_weapon_primary_load(&tables,"Shotgun",2*1024*1024,&shotgun));
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    campaign_player_view.flags_7c=8;campaign_player_view.linked_handle=-1;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    campaign_player_damage.state.effects.handle=campaign_player_object.handle;
    campaign_player_damage.state.effects.health=campaign_player_damage.state.effects.class_health=10000;
    campaign_player_damage.state.effects.affiliation=1;
    for(i=0;i<11;i++)campaign_player_damage.factors[i]=1;
    scene_actor_body.allocated_bytes=1;scene_actor_body.spheres.items=&player_sphere;scene_actor_body.spheres.count=1;
    memcpy(scene_actor_body.state.orientation,identity,36);memcpy(scene_actor_body.state.local_tensor,identity,36);
    collision.spheres=physics.collision;collision.count=physics.sphere_count;collision.radius=physics.radius;
    CHECK(!scene_driller_entry_exit_open(&entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,resources,&physics,&collision,eye));
    CHECK(!scene_driller_damage_runtime_open(&host,&entry,&player,&prototype));
    scene.collision=&world;
    for(i=0;i<3;i++)start[i]=origin[i]+physics.spheres[0].center[i];start[2]-=20;
    campaign_pistol=sniper;before=host.damage.state.effects.health;
    CHECK(sniper.damage_kind>=0&&prototype.factors[sniper.damage_kind]>0);
    CHECK(!scene_precision_fire(&scene,1,start,forward,0));
    CHECK(fabsf(host.damage.state.effects.health-(before-sniper.damage*prototype.factors[sniper.damage_kind]))<.001f);
    /* Real face covers the chassis; sniper stops, authored rail pierces it. */
    face.vertices=vertices;face.count=4;face.plane[2]=-1;face.plane[3]=-10;
    for(i=0;i<3;i++){face.minimum[i]=vertices[0][i];face.maximum[i]=vertices[2][i];
        mover.input_matrix[i][i]=mover.output_matrix[i][i]=1;}
    mover.minimum[0]=-5;mover.maximum[0]=5;mover.minimum[1]=8;mover.maximum[1]=14;
    mover.minimum[2]=-11;mover.maximum[2]=-9;mover.flat_faces=&face;mover.flat_count=1;
    campaign_movers.views=&mover;campaign_movers.count=1;before=host.damage.state.effects.health;
    CHECK(!scene_precision_fire(&scene,2,start,forward,0));CHECK(host.damage.state.effects.health==before);
    campaign_pistol=rail;CHECK(rail.damage_kind>=0&&prototype.factors[rail.damage_kind]>0);
    CHECK(!scene_precision_fire(&scene,3,start,forward,1));
    CHECK(fabsf(host.damage.state.effects.health-(before-rail.damage*prototype.factors[rail.damage_kind]))<.001f);
    /* Rail also reaches an actual registered body beyond the host. Only NPC
     * damage publication is redirected; host collision/damage remains real. */
    npc.body.allocated_bytes=1;npc.body.spheres.items=&npc_sphere;npc.body.spheres.count=1;
    memcpy(npc.body.state.orientation,identity,36);memcpy(npc.body.state.position,start,12);npc.body.state.position[2]=20;
    npc.damage.effects.health=100;npc.view.weapons[0]=-1;
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&npc.view,&npc.registration));
    campaign_npc_bodies=&npc;campaign_npc_body_count=1;before=host.damage.state.effects.health;
    CHECK(!recorded_precision_fire(&scene,4,start,forward,1));
    CHECK(npc_publications==1&&npc_published_handle==npc.registration.handle);
    CHECK(host.damage.state.effects.health<before&&!host.damage.destroyed);
    campaign_npc_bodies=NULL;campaign_npc_body_count=0;campaign_movers.count=0;
    /* No spread here: all authored pellets cross the same real chassis line.
     * Bullet immunity must still consume each pellet before the player. */
    shotgun.ai_spread_degrees=0;CHECK(shotgun.damage_kind==1&&prototype.factors[1]==0);
    memcpy(shooter.eye_position,start,12);shooter.registration.handle=UINT32_MAX;
    memcpy(scene_actor_body.state.position,start,12);scene_actor_body.state.position[2]=20;
    for(i=0;i<3;i++){
        /* combat_body first rejects against the committed world-space AABB;
         * this fixture relocates directly rather than running the body solver. */
        scene_actor_body.state.bounds.minimum[i]=scene_actor_body.state.position[i]-player_sphere.radius;
        scene_actor_body.state.bounds.maximum[i]=scene_actor_body.state.position[i]+player_sphere.radius;
        delta[i]=scene_actor_body.state.position[i]-start[i];
    }
    before=host.damage.state.effects.health;player_before=campaign_player_damage.state.effects.health;prior=host.damage_calls;
    CHECK(!campaign_enemy_shotgun_fire(&scene,&shooter,NULL,UINT32_MAX,&shotgun,delta,60,0,&effects,&feedback,&amount));
    CHECK(amount==0&&host.damage.state.effects.health==before&&campaign_player_damage.state.effects.health==player_before);
    CHECK(host.damage_calls==prior+shotgun.projectiles);
    /* Move the actual host off the ray: the same scene helper now hits player. */
    physics.state.position[0]+=40;
    CHECK(!campaign_enemy_shotgun_fire(&scene,&shooter,NULL,UINT32_MAX,&shotgun,delta,60,0,&effects,&feedback,&amount));
    CHECK(amount>0&&campaign_player_damage.state.effects.health<player_before);
    scene_driller_damage_runtime_close(&host);CHECK(!scene_driller_entry_exit_close(&entry,1));
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&npc.registration));
    scene_driller_resources_close(&resources);rf_vpp_close(&tables);rf_vpp_close(&meshes);
    for(i=0;i<4;i++)rf_vpp_close(maps+i);
    puts("installed Driller: sniper damage/cover, rail wall+host+NPC continuation, shotgun immunity blocks actor; NPC precision damage recorded seam");
    return 0;
}
