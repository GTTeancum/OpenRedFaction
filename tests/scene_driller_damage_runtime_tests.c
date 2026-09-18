/* Installed host resources + real scene registration, damage and exit adapter.
 * Only the world provider is controlled: blocked passage versus clear floor. */
#include <stdio.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"Driller runtime line%d: %s\n",__LINE__,#x);return 1;} } while(0)
typedef struct exit_world {uint32_t blocked,paths,floors;} exit_world;
static int exit_query(void *context,const rf_collision_body_query *q,rf_geometry_body_hit *hit,uint32_t *found)
{
    exit_world *world=context;memset(hit,0,sizeof(*hit));*found=0;
    if(q->end[1]<q->start[1]-.01f && fabsf(q->end[0]-q->start[0])<.001f && fabsf(q->end[2]-q->start[2])<.001f){
        ++world->floors;*found=1;hit->contact.fraction=.1f;hit->contact.normal[1]=1;
    }else {++world->paths;if(world->blocked){*found=1;hit->contact.fraction=.25f;hit->contact.normal[0]=1;}}
    return RF_OK;
}
int main(void)
{
    const float identity[9]={1,0,0,0,1,0,0,0,1},origin[3]={0,10,0},eye[3]={0,0,0};
    rf_vpp tables={0},meshes={0},maps[4]={{0}};scene_driller_resources *resources=NULL;
    scene_driller_physics physics;scene_driller_damage prototype;scene_driller_damage_runtime owner={0};
    scene_driller_entry_exit entry={0};scene_driller_player_adapter player={0};scene_vehicle_collision collision={0};
    rf_physics_sphere player_sphere={{0,0,0},.4f,0,0};exit_world world={0};
    rf_weapon_flight_contact contact={0},prior,saved;float start[3],delta[3]={0,0,40},amount,health,fraction;
    uint32_t i,matched,handled,changed,liquid,source,old_handle;char path[128];
    rf_damage_request lethal={10000,0,3,0,UINT32_MAX,0};
    CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;++i){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!scene_driller_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,4*1024*1024,&resources));
    CHECK(!scene_driller_physics_initialize(resources,origin,identity,9.8f,&physics));
    CHECK(!scene_driller_damage_open(&prototype,&tables,resources,1,0,2,2*1024*1024));
    CHECK(prototype.state.effects.health==900 && prototype.state.effects.armor==0);
    CHECK(prototype.factors[0]==0 && prototype.factors[1]==0 && prototype.factors[2]==.1f && prototype.factors[3]==1);
    printf("Driller authored object_flags=%08x class_flags=%08x bash=%g bullet=%g AP=%g explosive=%g\n",
        prototype.object_flags,prototype.class_flags,prototype.factors[0],prototype.factors[1],prototype.factors[2],prototype.factors[3]);
    rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object));
    source=campaign_player_object.handle;campaign_player_view.linked_handle=-1;campaign_player_view.flags_7c=8;
    campaign_player_damage.state.effects.handle=source;campaign_player_damage.state.effects.health=100;
    campaign_player_damage.state.effects.affiliation=1;rf_scene_actor_eye_enabled=0;
    scene_actor_body.spheres.items=&player_sphere;scene_actor_body.spheres.count=1;
    memcpy(scene_actor_body.state.position,origin,12);memcpy(scene_actor_body.state.orientation,identity,36);
    memcpy(scene_actor_body.state.local_tensor,identity,36);
    collision.spheres=physics.collision;collision.count=physics.sphere_count;collision.radius=physics.radius;
    collision.query=exit_query;collision.query_context=&world;
    CHECK(!scene_driller_entry_exit_open(&entry,&campaign_registry,&campaign_entities,&campaign_player_view,
        &scene_actor_body,resources,&physics,&collision,eye));
    CHECK(!scene_driller_damage_runtime_open(&owner,&entry,&player,&prototype));
    CHECK(owner.damage.state.effects.handle==entry.host.handle && scene_driller_damage_runtime_valid(&owner));
    /* A ray through an actual authored sphere, not an invented vehicle AABB. */
    for(i=0;i<3;++i)start[i]=origin[i]+physics.spheres[0].center[i];
    start[2]-=20;
    CHECK(!scene_driller_firearm_select(source,start,delta,1,&contact,&matched) && matched);
    CHECK(contact.face==entry.host.handle && contact.object==SCENE_DRILLER_PROJECTILE_OWNER);
    fraction=contact.hit.fraction;saved=contact;
    prior=contact;prior.object=123;liquid=1;matched=1;
    CHECK(!scene_driller_projectile_compose(source,start,delta,0,&prior,&liquid,&matched));
    CHECK(prior.object==123 && liquid==1); /* equal world contact wins */
    prior.hit.fraction=fraction*.5f;
    CHECK(!scene_driller_projectile_compose(source,start,delta,0,&prior,&liquid,&matched) && prior.object==123);
    CHECK(!scene_driller_firearm_select(entry.host.handle,start,delta,1,&prior,&matched) && !matched);
    CHECK(!scene_driller_projectile_damage(&saved,source,100,1,1,&handled,&amount) && handled);
    CHECK(fabsf(amount-100*prototype.factors[1])<.001f);
    CHECK(fabsf(owner.damage.state.effects.health-(900-amount))<.001f);
    CHECK(!scene_driller_projectile_damage(&saved,source,100,2,2,&handled,&amount) && handled && amount==10);
    /* SP responsible_handle is death credit, not a last-hit field. */
    CHECK(owner.damage.state.responsible_handle==UINT32_MAX);
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed) && changed && entry.session.active);
    CHECK(entry.host.driver==source && campaign_player_view.linked_handle==(int32_t)entry.host.handle);
    CHECK(!scene_driller_firearm_select(source,start,delta,1,&prior,&matched) && !matched);
    lethal.source=source;
    CHECK(!scene_driller_damage_runtime_receive(&owner,entry.host.handle,&lethal,3,&amount));
    CHECK(owner.damage.destroyed && !entry.host.alive && owner.last_destroyer==source);
    CHECK(owner.damage.state.responsible_handle==source);
    world.blocked=1;
    CHECK(!scene_driller_damage_runtime_tick(&owner,&changed) && !changed);
    CHECK(entry.session.active && entry.host.driver==source && owner.eject_pending && world.paths>=4);
    world.blocked=0;
    CHECK(!scene_driller_damage_runtime_tick(&owner,&changed) && changed);
    CHECK(!entry.session.active && entry.host.driver==UINT32_MAX && campaign_player_view.linked_handle==-1);
    CHECK(!owner.eject_pending && owner.exit_count==1 && world.floors>0);
    CHECK(!scene_driller_damage_runtime_tick(&owner,&changed) && !changed && owner.exit_count==1);
    /* A stale contact cannot damage a new generation in the same registry. */
    old_handle=entry.registration.handle;health=owner.damage.state.effects.health;
    CHECK(!rf_entity_view_unregister(&campaign_registry,&campaign_entities,&entry.registration));
    CHECK(!rf_entity_view_register(&campaign_registry,&campaign_entities,&entry.view,&entry.registration));
    CHECK(entry.registration.handle!=old_handle && !scene_driller_damage_runtime_valid(&owner));
    CHECK(!scene_driller_firearm_select(source,start,delta,1,&prior,&matched) && !matched);
    CHECK(!scene_driller_projectile_damage(&saved,source,100,-1,4,&handled,&amount) && handled && amount==0);
    CHECK(owner.damage.state.effects.health==health);
    scene_driller_damage_runtime_close(&owner);CHECK(!scene_driller_entry_exit_close(&entry,1));
    scene_driller_resources_close(&resources);rf_vpp_close(&tables);rf_vpp_close(&meshes);
    for(i=0;i<4;++i)rf_vpp_close(maps+i);
    puts("Driller runtime: authored900HP, source damage, retained blocked driver, safe release, nearest/self/stale checks");
    return 0;
}
