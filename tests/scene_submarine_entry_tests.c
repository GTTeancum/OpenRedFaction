#include <stdio.h>
#include "../src/diagnostic/scene_driller_resources.inc"
#include "../src/diagnostic/scene_driller_physics.inc"
#include "../src/diagnostic/scene_vehicle_possession.inc"
static int campaign_body_query_batch_for(const rf_geometry_collision_world *w,const rf_collision_body_query *q,
    rf_geometry_body_hit *h,uint32_t *found,void *batch,const rf_geometry_collision_movers *m)
{(void)w;(void)q;(void)h;(void)batch;(void)m;return RF_RANGE;}
#include "../src/diagnostic/scene_vehicle_collision.inc"
#include "../src/diagnostic/scene_driller_entry_exit.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sub entry line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct fixture {uint32_t wet,blocked,queries,water_calls;} fixture;
static int sweep(void *v,const rf_collision_body_query *q,rf_geometry_body_hit *h,uint32_t *found)
{fixture *f=v;(void)q;(void)h;++f->queries;*found=f->blocked;return RF_OK;}
static int wet(void *v,const rf_collision_body_sphere *s,uint32_t count,const scene_vehicle_pose *a,
    const scene_vehicle_pose *b,uint32_t *allowed)
{fixture *f=v;(void)a;(void)b;if(!s||count!=1||s[0].radius!=.5f)return RF_RANGE;
 ++f->water_calls;*allowed=f->wet;return RF_OK;}
int main(void)
{
    static rf_object_registry objects;static rf_entity_registry entities;
    rf_entity_view player={0};rf_registered_entity_view registered={0};rf_physics_body body={0};
    rf_physics_sphere sphere={{0,0,0},.5f,0,0};scene_driller_resources resource={0};
    scene_driller_physics physics={0};scene_vehicle_collision collision={0};scene_driller_entry_exit entry={0};
    rf_model_attachment tag={0};fixture f={1,0,0,0};uint32_t changed;scene_vehicle_pose before;
    rf_object_registry_init(&objects);player.linked_handle=-1;
    CHECK(!rf_entity_view_register(&objects,&entities,&player,&registered));
    tag.rotation[3]=1;strcpy(tag.name,"interface_1");resource.tags.items=&tag;resource.tags.count=1;
    resource.physics.authored.movement_index=7;resource.physics.authored.use_kind=1;resource.physics.authored.use_radius=5;
    resource.movement.speed=6;body.spheres.items=&sphere;body.spheres.count=1;body.state.position[0]=3;
    body.state.orientation[0]=body.state.orientation[4]=body.state.orientation[8]=1;
    physics.state.orientation[0]=physics.state.orientation[4]=physics.state.orientation[8]=1;
    physics.sphere_count=1;physics.collision[0].radius=2;physics.radius=2;
    collision.spheres=physics.collision;collision.count=1;collision.radius=2;collision.query=sweep;collision.query_context=&f;
    CHECK(!scene_driller_entry_exit_open(&entry,&objects,&entities,&player,&body,&resource,&physics,&collision,(float[3]){0,0,0}));
    CHECK(scene_driller_entry_exit_use(&entry,1,1,&changed)==RF_RANGE && !entry.session.active);
    entry.held=0;entry.water_path=wet;entry.water_context=&f;f.wet=0;
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed)&&!changed&&!entry.session.active);
    entry.held=0;f.wet=1;
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed)&&changed&&entry.session.active);
    before=entry.player.pose;entry.held=0;f.wet=0;
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed)&&!changed&&entry.session.active);
    CHECK(!memcmp(&before,&entry.player.pose,sizeof(before)));
    entry.held=0;f.wet=1;f.blocked=1;
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed)&&!changed&&entry.session.active);
    entry.held=0;f.blocked=0;f.queries=0;
    CHECK(!scene_driller_entry_exit_use(&entry,1,1,&changed)&&changed&&!entry.session.active);
    CHECK(f.queries==1 && entry.player.pose.position[0]>2.5f && player.linked_handle==-1);
    CHECK(!scene_driller_entry_exit_close(&entry,1));CHECK(!rf_entity_view_unregister(&objects,&entities,&registered));
    puts("PASS submarine wet boarding, dry/blocked exit retention and floorless swimming exit");return 0;
}
